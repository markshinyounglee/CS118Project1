#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <stdio.h>
#include <errno.h>

#include "transport.h"

packet tosend[MAX_SEQ] = {0};
packet torecv[MAX_SEQ] = {0};

struct timeval start; // Last packet sent at this time
struct timeval now; // Temp for current time

int seq = 1; // Last sent packet into tosend
int ready = 0; // Last placed packet into tosend
int base_seq = 1; // Last acked by client packet in tosend
int ack = 0; // Last acked packet in torecv by server
int force_ack = 0; // Send ACK without data

int sec_flag = 0;
int sec_state = INIT;
int sec_mac = 0;
int force_sec_mac = 0;

void process_security(packet* pkt);
size_t encrypt_data(char *data, size_t size, char *iv, char *cipher, int using_mac);
size_t decrypt_cipher(char *cipher, size_t size, char *iv, char *data, int using_mac);
void hmac(char* data, size_t size, char* digest);

size_t encode_data(char* buffer) {
    security* msg = (security*) buffer;
    msg->msg_type = DATA;
    data* dat = (data*) msg->data;

    char input[MAX_DATA_SIZE];

    int bytes_read;
    if (sec_mac)
        bytes_read = read(STDIN_FILENO, input, (MAX_DATA_SIZE / 16) * 15 - MAC_SIZE);
    else
        bytes_read = read(STDIN_FILENO, input, (MAX_DATA_SIZE / 16) * 15);
    if (bytes_read <= 0) return 0;
    char* iv = dat->iv;
    char* data = dat->data;
    size_t pay_size = encrypt_data(input, bytes_read, iv, data, sec_mac);
    if (sec_mac) {
        hmac(iv, pay_size + IV_SIZE, dat->data + pay_size);
    }

    fprintf(stderr, sec_mac ? "MAC %d %ld\n" : "ENC %d %ld\n", bytes_read, pay_size);
    dat->payload_size = htons(pay_size);

    msg->msg_len = htons(SECURITY_HEADER_SIZE + DATA_HEADER_SIZE + pay_size + (sec_mac ? MAC_SIZE : 0));

    return SECURITY_HEADER_SIZE + DATA_HEADER_SIZE + pay_size + (sec_mac ? MAC_SIZE : 0);
}

void decode_data(data* data) {
    char output[MAX_DATA_SIZE];

    char* iv = data->iv;
    char* dat = data->data;
    if (sec_mac) {
        char digest[MAC_SIZE];
        hmac(iv, IV_SIZE + ntohs(data->payload_size), digest);
        if (memcmp(digest, dat + ntohs(data->payload_size), MAC_SIZE) != 0) {
            exit(4);
        }
    }
    size_t pay_size = decrypt_cipher(dat, ntohs(data->payload_size), iv, output, sec_mac);
    fprintf(stderr, sec_mac ? "MAC DEC %ld %d\n" : "DEC %ld %d\n", pay_size, ntohs(data->payload_size));
    write(STDOUT_FILENO, output, pay_size);
}

void process_packet(packet* pkt) {
    if (!sec_flag) { write(STDOUT_FILENO, pkt->data, ntohs(pkt->size)); return; }
    process_security(pkt);
}

void make_packet(char* data, int len) {
    if (sec_flag && sec_state != NORMAL && !data) return;
    char buffer[MSS];
    char* output = buffer;
    int bytes_read;

    if (data) {
        bytes_read = len;
        output = data;
    } else {
        if (sec_state == NORMAL) {
            bytes_read = encode_data(buffer);
        } else {
            bytes_read = read(STDIN_FILENO, buffer, MSS);
        }
    }

    if (bytes_read > 0) {
        tosend[ready].seq = htonl(ready + 1);
        tosend[ready].ack = htonl(ack);
        tosend[ready].size = htons(bytes_read);
        memcpy(&tosend[ready].data, output, bytes_read);
        ready++;
    }
}

void recv_data(packet* pkt) {
    if (ntohl(pkt->seq) >= MAX_SEQ) { fprintf(stderr, "INVALID SEQ %d\n", ntohl(pkt->seq)); return; }
    if (ntohl(pkt->seq) != 0) torecv[ntohl(pkt->seq) - 1] = *pkt;

    // If ack received, then we can move window
    if (base_seq < ntohl(pkt->ack) + 1) {
        gettimeofday(&start, NULL);
        base_seq = ntohl(pkt->ack) + 1;
    }

    // Attempt to process packets in order (skipping if packet isn't ready)
    for (int i = ack; i < MAX_SEQ; i++) {
        if (ntohl(torecv[i].seq) != 0) {
            process_packet(&torecv[i]);
            ack++;
        } else {
            break;
        }
    }

    // Check if there's a next packet to send
    if (ntohl(tosend[seq - 1].seq) != 0) {
        // Set ACK
        tosend[seq - 1].ack = htonl(ack);

    // If not, we need to send just an ACK (if didn't receive just ACK)
    } else if (ntohl(pkt->seq) != 0) {
        force_ack = 1;
    }
}

void listen_loop(int sockfd, struct sockaddr_in* addr) {
    /* RTO args */
    gettimeofday(&now, NULL);
    gettimeofday(&start, NULL);

    /* Create buffer to store incoming data */
    packet buffer = {0};
    socklen_t addr_size = sizeof(struct sockaddr);

    // Loop
    while (1) {
        make_packet(NULL, 0);

        /* Listen for data from clients */
        // Receive only packet header
        int bytes_recvd = recvfrom(sockfd, &buffer, sizeof(buffer),
                                // socket  store data  how much
                                0, (struct sockaddr*) addr,
                                &addr_size);

        /* Inspect data from client */
        if (bytes_recvd > 0) {
            // Receive rest of bytes
            fprintf(stderr, "RECV %d ACK %d SIZE %d \n", ntohl(buffer.seq), ntohl(buffer.ack), ntohs(buffer.size));
            recv_data(&buffer);
            continue;
        }

        /* Send data back to client */

        // Send packets in window
        buffer = tosend[seq - 1];
        if (ntohl(buffer.seq) != 0 && seq < base_seq + CWND) {
            buffer.ack = htonl(ack);
            fprintf(stderr, "SEND %d ACK %d SIZE %d\n", ntohl(buffer.seq), ntohl(buffer.ack), ntohs(buffer.size));
            size_t size = PACKET_HEADER_SIZE + ntohs(buffer.size);
            int did_send = sendto(sockfd, &buffer, size,
                            // socket  send data   how much to send
                                0, (struct sockaddr*) addr,
                            // flags   where to send
                                sizeof(struct sockaddr_in));
            seq++;
        } else if (force_ack) {
            force_ack = 0;
            memset(&buffer, 0, sizeof(buffer));
            buffer.ack = htonl(ack);
            fprintf(stderr, "SEND %d ACK %d SIZE %d\n", ntohl(buffer.seq), ntohl(buffer.ack), ntohs(buffer.size));
            int did_send = sendto(sockfd, &buffer, PACKET_HEADER_SIZE,
                            // socket  send data   how much to send
                                0, (struct sockaddr*) addr,
                            // flags   where to send
                                sizeof(struct sockaddr_in));
        }

        // Check if timer went off
        gettimeofday(&now, NULL);
        if (TV_DIFF(now, start) >= RTO && base_seq < seq) {
            buffer = tosend[base_seq - 1];
            buffer.ack = htonl(ack);
            fprintf(stderr, "RTO SEND %d ACK %d SIZE %d DIAG %d %d\n", ntohl(buffer.seq), ntohl(buffer.ack), ntohs(buffer.size), base_seq, seq);
            size_t size = PACKET_HEADER_SIZE + ntohs(buffer.size);
            int did_send = sendto(sockfd, &buffer, size,
                                // socket  send data   how much to send
                                    0, (struct sockaddr*) addr,
                                // flags   where to send
                                    sizeof(struct sockaddr_in));
            gettimeofday(&start, NULL);
        }
    }
}