#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <sys/socket.h>
#include "consts.h"

extern packet tosend[MAX_SEQ];
extern packet torecv[MAX_SEQ];

extern int seq; // Last sent packet into tosend
extern int ready; // Last placed packet into tosend
extern int base_seq; // Last acked by client packet in tosend
extern int ack; // Last acked packet in torecv by server
extern int force_ack; // Send ACK without data

void listen_loop(int sockfd, struct sockaddr_in* addr);
void make_packet(char* data, int len);
void decode_data(data* data);

#endif