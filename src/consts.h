#ifndef CONSTS_H
#define CONSTS_H

/* VARIABLES */

// Maximum segment size
#define MSS 1024

// Retransmission time
#define TV_DIFF(end, start) (end.tv_sec * 1000000) - (start.tv_sec * 1000000) + end.tv_usec - start.tv_usec
#define RTO 1000000

// Window sizes
#define CWND 20
#define MAX_SEQ 4000

// Security sizes
#define NONCE_SIZE 32
#define SECRET_SIZE 32
#define MAC_SIZE 32
#define IV_SIZE 16

// Header sizes
#define PACKET_HEADER_SIZE 12
#define SECURITY_HEADER_SIZE 4
#define CERT_HEADER_SIZE 4
#define SERVER_HELLO_HEADER_SIZE (4 + NONCE_SIZE)
#define KEY_EX_HEADER_SIZE 4
#define DATA_HEADER_SIZE (4 + IV_SIZE)

// Data sizes
#define MAX_SECURITY_DATA_SIZE (MSS - SECURITY_HEADER_SIZE)
#define MAX_SERVER_HELLO_DATA_SIZE (MAX_SECURITY_DATA_SIZE - SERVER_HELLO_HEADER_SIZE)
#define MAX_KEY_EX_DATA_SIZE (MAX_SECURITY_DATA_SIZE - KEY_EX_HEADER_SIZE)
#define MAX_DATA_SIZE (MAX_SECURITY_DATA_SIZE - DATA_HEADER_SIZE)

// Security message types
#define CLIENT_HELLO 1
#define SERVER_HELLO 2
#define KEY_EXCHANGE_REQUEST 16
#define FINISHED 20
#define DATA 255

// Cipher communication types
#define ENCRYPTION_ONLY 0
#define ENCRYPT_THEN_MAC 1

// Client states
#define INIT 0
#define AWAIT_HELLO 1
#define AWAIT_FINISHED 2
#define NORMAL 3

// Server states
// INIT
#define AWAIT_KEY_EXCHANGE 4
// NORMAL

/* STRUCTS */

typedef struct {
    int seq;
    int ack;
    short size;
    char padding[2];
    char data[MSS];
} packet;

typedef struct {
    unsigned char msg_type;
    char padding;
    short msg_len;
    char data[MAX_SECURITY_DATA_SIZE];
} security;

typedef struct {
    char comm_type;
    char padding[3];
    char nonce[NONCE_SIZE];
} client_hello;

typedef struct {
    char comm_type;
    char sig_size;
    short cert_size;
    char nonce[NONCE_SIZE];
    short key_len;
    char padding[2];
    // Public Key + Key Signature + Nonce Signature
    char data[MAX_SERVER_HELLO_DATA_SIZE];
} server_hello;

typedef struct {
    char padding;
    char sig_size;
    short cert_size;
    short key_len;
    char padding2[2];
    // Public Key + Key Signature + Nonce Signature
    char data[MAX_KEY_EX_DATA_SIZE];
} key_exchange_request;

typedef struct {} finished;

typedef struct {
    short payload_size;
    char padding[2];
    char iv[IV_SIZE];
    // Payload + MAC (if enabled)
    char data[MAX_DATA_SIZE];
} data;

#endif