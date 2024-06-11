#include <sys/socket.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>

#include "consts.h"
#include "security.h"

void listen_loop(int sockfd, struct sockaddr_in* addr);
void make_packet(char* data, int len);
void decode_data(data* data);

void generate_hello(client_hello* hello) {
    fprintf(stderr, "RECV HELLO\n");
    security msg = {0};
    msg.msg_type = SERVER_HELLO;
    server_hello* s_hello = (server_hello*) msg.data;

    fprintf(stderr, hello->comm_type ? "REQ MAC\n" : "REQ ENC\n");
    s_hello->comm_type = sec_mac ? (hello->comm_type ? ENCRYPT_THEN_MAC : ENCRYPTION_ONLY) : ENCRYPTION_ONLY;
    sec_mac = s_hello->comm_type;
    fprintf(stderr, sec_mac ? "USE MAC\n" : "USE ENC\n");

    // Server nonce
    generate_nonce(s_hello->nonce, NONCE_SIZE);
    nonce = malloc(NONCE_SIZE);
    memcpy(nonce, s_hello->nonce, NONCE_SIZE);

    // Create certificate
    s_hello->key_len = htons(pub_key_size);
    // Add public key
    memcpy(s_hello->data, public_key, pub_key_size);
    // Adding signature
    size_t cert_sig_size = cert_size - CERT_HEADER_SIZE - pub_key_size;
    size_t total_cert_size = cert_size;
    memcpy(s_hello->data + pub_key_size, 
           certificate + CERT_HEADER_SIZE + pub_key_size,
           cert_sig_size);
    s_hello->cert_size = htons(total_cert_size);

    // Sign and add client nonce
    char signed_nonce[255];
    size_t sig_size = sign(hello->nonce, NONCE_SIZE, signed_nonce);
    s_hello->sig_size = sig_size;
    memcpy(s_hello->data + pub_key_size + cert_sig_size, signed_nonce, sig_size);

    msg.msg_len = htons(SERVER_HELLO_HEADER_SIZE + total_cert_size + sig_size);

    make_packet((char*) &msg, SECURITY_HEADER_SIZE + SERVER_HELLO_HEADER_SIZE + total_cert_size + sig_size);

    sec_state = AWAIT_KEY_EXCHANGE;
    fprintf(stderr, "SEND HELLO\n");
}

void do_key_exchange(key_exchange_request* ker) {
    fprintf(stderr, "RECV KEY EX\n");
    security msg = {0};
    msg.msg_type = FINISHED;
    msg.msg_len = 0;

    // Import client key
    load_peer_public_key(ker->data, ntohs(ker->key_len));

    // Verify certificate
    char* client_pub_key = ker->data;
    char* client_pub_key_sig = ker->data + ntohs(ker->key_len);
    size_t sig_size = ntohs(ker->cert_size) - (ntohs(ker->key_len) + CERT_HEADER_SIZE);

    // Check if certificate was signed by client
    if (verify(client_pub_key, ntohs(ker->key_len), client_pub_key_sig, sig_size, ec_peer_public_key) != 1) {
        exit(2);
    }

    // Verify nonce
    char* signed_nonce = ker->data + ntohs(ker->cert_size) - CERT_HEADER_SIZE;

    if (verify(nonce, NONCE_SIZE, signed_nonce, ker->sig_size, ec_peer_public_key) != 1) {
        exit(3);
    }

    // Derive secret
    derive_secret();
    if (sec_mac) derive_keys();

    make_packet((char*) &msg, SECURITY_HEADER_SIZE);

    sec_state = NORMAL;
    fprintf(stderr, "SEND FINISHED\n");
}

void process_security(packet* pkt) {
    security* sec_data = (security*) pkt->data;

    switch (sec_state) {
        case INIT: {
            if (sec_data->msg_type != CLIENT_HELLO) exit(1);
            client_hello* hello = (client_hello*) sec_data->data;
            generate_hello(hello);
            break;
        }
        case AWAIT_KEY_EXCHANGE: {
            if (sec_data->msg_type != KEY_EXCHANGE_REQUEST) exit(1);
            key_exchange_request* ker = (key_exchange_request*) sec_data->data;
            do_key_exchange(ker);
            break;
        }
        case NORMAL: {
            if (sec_data->msg_type != DATA) exit(1);
            data* dat = (data*) sec_data->data;
            decode_data(dat);
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: server <security mode> <port> <private key> <certificate>\n");
        exit(1);
    }

    // Set security mode
    if (argv[1][0] == '1') sec_flag = 1;

    if (sec_flag && argc < 5) {
        fprintf(stderr, "Usage: server 1 <port> <private key> <certificate>\n");
        exit(1);
    }

    // Not part of official spec, but just for testing
    if (argc >= 6 && argv[5][0] == '0') {
        sec_mac = 0;
    } else {
        // Default to enabled; this is negotiated from client
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

    /* Construct our address */
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET; // use IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY; // accept all connections
                            // same as inet_addr("0.0.0.0") 
                                     // "Address string to network bytes"
    // Set receiving port
    int PORT = atoi(argv[2]);
    server_addr.sin_port = htons(PORT); // Big endian

    /* Let operating system know about our config */
    int did_bind = bind(sockfd, (struct sockaddr*) &server_addr, 
                        sizeof(server_addr));
    // Error if did_bind < 0 :(
    if (did_bind < 0) return errno;

    struct sockaddr_in client_addr; // Same information, but about client
    socklen_t s = sizeof(struct sockaddr_in);
    char buffer;

    if (sec_flag) {
        load_private_key(argv[3]);
        load_certificate(argv[4]);
        derive_public_key();
    }

    // Wait for client connection
    while (1) {
        int bytes_recvd = recvfrom(sockfd, &buffer, sizeof(buffer), 
                                MSG_PEEK, (struct sockaddr*) &client_addr, 
                                &s);
        if (bytes_recvd > 0) break;
    }
    
    listen_loop(sockfd, &client_addr);
    
    return 0;
}