#include <sys/socket.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#include "consts.h"
#include "security.h"

void listen_loop(int sockfd, struct sockaddr_in* addr);
void make_packet(char* data, int len);
void decode_data(data* data);

void generate_hello() {
    security msg = {0};
    msg.msg_type = CLIENT_HELLO;
    msg.msg_len = htons(sizeof(client_hello));
    client_hello* hello = (client_hello*) msg.data;

    hello->comm_type = sec_mac ? ENCRYPT_THEN_MAC : ENCRYPTION_ONLY;
    fprintf(stderr, sec_mac ? "REQ MAC\n" : "REQ ENC\n");

    generate_nonce(hello->nonce, NONCE_SIZE);
    nonce = malloc(NONCE_SIZE);
    memcpy(nonce, hello->nonce, NONCE_SIZE);

    make_packet((char*) &msg, SECURITY_HEADER_SIZE + sizeof(client_hello));

    sec_state = AWAIT_HELLO;
    fprintf(stderr, "SEND HELLO\n");
}

void generate_key_exchange(server_hello* hello) {
    fprintf(stderr, "RECV HELLO\n");
    security msg = {0};
    msg.msg_type = KEY_EXCHANGE_REQUEST;
    key_exchange_request* ker = (key_exchange_request*) msg.data;

    // Check for MAC
    sec_mac = hello->comm_type ? ENCRYPT_THEN_MAC : ENCRYPTION_ONLY;
    fprintf(stderr, sec_mac ? "USE MAC\n" : "USE ENC\n");

    // grading
    if (force_sec_mac && !sec_mac) exit(12);

    // Verify certificate
    char* server_pub_key = hello->data;
    char* server_pub_key_sig = hello->data + ntohs(hello->key_len);
    size_t sig_size = ntohs(hello->cert_size) - (ntohs(hello->key_len) + CERT_HEADER_SIZE);

    // Check if certificate was signed by CA
    if (verify(server_pub_key, ntohs(hello->key_len), server_pub_key_sig, sig_size, ec_ca_public_key) != 1) {
        exit(2);
    }

    // Add peer key
    load_peer_public_key((char*) server_pub_key, ntohs(hello->key_len));

    // Verify nonce
    char* signed_nonce = hello->data + ntohs(hello->cert_size) - CERT_HEADER_SIZE;

    if (verify(nonce, NONCE_SIZE, signed_nonce, hello->sig_size, ec_peer_public_key) != 1) {
        exit(3);
    }

    derive_secret();
    if (sec_mac) derive_keys();

    // Copy public key
    memcpy(ker->data, public_key, pub_key_size);
    ker->key_len = htons(pub_key_size);

    // Sign public key
    sig_size = sign(public_key, pub_key_size, ker->data + pub_key_size);
    size_t total_cert_size = CERT_HEADER_SIZE + pub_key_size + sig_size;
    ker->cert_size = htons(total_cert_size);

    // Sign nonce
    char signed_nonce2[255];
    size_t sig_nonce_size = sign(hello->nonce, NONCE_SIZE, signed_nonce2);
    ker->sig_size = sig_nonce_size;
    memcpy(ker->data + pub_key_size + sig_size, signed_nonce2, sig_nonce_size);

    msg.msg_len = htons(KEY_EX_HEADER_SIZE + total_cert_size + sig_nonce_size);

    make_packet((char*) &msg, SECURITY_HEADER_SIZE + KEY_EX_HEADER_SIZE + total_cert_size + sig_nonce_size);

    sec_state = AWAIT_FINISHED;
    fprintf(stderr, "SEND KEY EX\n");
}

void process_security(packet* pkt) {
    security* sec_data = (security*) pkt->data;

    switch (sec_state) {
        case AWAIT_HELLO: {
            if (sec_data->msg_type != SERVER_HELLO) exit(1);
            server_hello* hello = (server_hello*) sec_data->data;
            generate_key_exchange(hello);
            break;
        }
        case AWAIT_FINISHED: {
            fprintf(stderr, "RECV FINISHED\n");
            if (sec_data->msg_type != FINISHED) exit(1);
            sec_state = NORMAL;
            break;
        }
        case NORMAL: {
            if (sec_data->msg_type != DATA) exit(1);
            data* dat = (data*) sec_data->data;
            decode_data(dat);
            break;
        }
        default: exit(1);
    }
}

int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: client <security mode> <hostname> <port> <certificate authority public key>\n");
        exit(1);
    }

    // Set security mode
    if (argv[1][0] == '1') sec_flag = 1;

    if (sec_flag && argc < 5) {
        fprintf(stderr, "Usage: client 1 <hostname> <port> <certificate authority public key>\n");
        exit(1);
    }

    // Not part of official spec, but just for testing
    if (argc >= 6 && argv[5][0] == '1') {
        force_sec_mac = 1;
        sec_mac = 1;
    }

    /* Create sockets */
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
                     // use IPv4  use UDP
    // Error if socket could not be created
    if (sockfd < 0) return errno;

    // Set socket for nonblocking
    int flags = fcntl(sockfd, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(sockfd, F_SETFL, flags);

    // Setup stdin for nonblocking
    flags = fcntl(STDIN_FILENO, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(STDIN_FILENO, F_SETFL, flags);

    /* Construct server address */
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET; // use IPv4
    char* addr = strcmp(argv[2], "localhost") == 0 ? "127.0.0.1" : argv[2];
    server_addr.sin_addr.s_addr = inet_addr(addr);
    // Set sending port
    int PORT = atoi(argv[3]);
    server_addr.sin_port = htons(PORT); // Big endian

    if (sec_flag) {
        load_ca_public_key(argv[4]);
        generate_private_key();
        derive_public_key();
        generate_hello();
    }
    listen_loop(sockfd, &server_addr);

    return 0;
}