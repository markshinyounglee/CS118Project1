#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>
#include <list>
#include <cstring>
#include "diagnostics.h" // delete after use

#define MAX_WINDOW 20240
#define DELAY_RETRANSMIT 1
#define MAX_DELAY_HOLD 3

// uncomment before submission
/*
#define MSS 1012 // MSS = Maximum Segment Size (aka max length)

typedef struct {
	uint32_t ack;
	uint32_t seq;
	uint16_t length;
	uint8_t flags;
	uint8_t unused;
	uint8_t payload[MSS];
} packet; */

using namespace std;

typedef struct { // sliding window
  list<packet> bufcontent; // payload length should be 20240
  uint32_t len; // check the length to be less than MAX_WINDOW
} ntbuf;

ntbuf sndbuf; // restricted to 20240 bytes
ntbuf rcvbuf; // resizable

clock_t start_clock = 0, end_clock = 0; // keep track of time elapsed

// for sending buffer, use resizable array (linked list)
// while ensuring that the sum of payloads stay within 20240 bytes
// for receiving buffer, receive as much as you want

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: server <port>\n");
    exit(1);
  }

  /* Create sockets */
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  // use IPv4  use UDP
  // Error if socket could not be created
  if (sockfd < 0)
    return errno;

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
  server_addr.sin_family = AF_INET;         // use IPv4
  server_addr.sin_addr.s_addr = INADDR_ANY; // accept all connections
                                            // same as inet_addr("0.0.0.0")
                                            // "Address string to network bytes"
  // Set receiving port
  int PORT = atoi(argv[1]);
  server_addr.sin_port = htons(PORT); // Big endian

  /* Let operating system know about our config */
  int did_bind =
      bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
  // Error if did_bind < 0 :(
  if (did_bind < 0)
    return errno;

  struct sockaddr_in client_addr; // Same information, but about client
  socklen_t s = sizeof(struct sockaddr_in);
  char buffer[MSS] = {0}; // use memset(buffer, 0, (MSS) * sizeof(char)) if this doesn't work
  char writebuf[MAX_WINDOW] = {0}; // what we will write in STDOUT


  int client_connected = 0;

  // 1. Perform handshake
  srand(time(NULL));
  uint32_t server_seq = 300; // rand(); // return random number for packet
  uint32_t ack;
  bool ack_recvd = false;
  packet received_pkt = {0};  // pkt variable we use
  packet sending_pkt = {0};
  int bytes_read = 0;
  int bytes_recvd = 0;
  int ack_counter = 1; 
  
  // Assume the socket has been set up with all other variables
  // phase 1: receive SYN packet from client
  while(1)
  {
    bytes_recvd = recvfrom(sockfd, &received_pkt, sizeof(received_pkt), 0,
                           (struct sockaddr*)&client_addr, &s);
    if (bytes_recvd <= 0 && !client_connected)
      continue;
    client_connected = 1; // At this point, the client has connected and sent data
    print_diag(&received_pkt, RECV);
    break;
  }
  // phase 2:
  // after receiving data packet from client, send the ACK packet
  // respond with 0 length payload
  sending_pkt = {
    .ack = htonl(ntohl(received_pkt.seq) + 1), // Make sure to convert to little endian
    .seq = htonl(server_seq),
    .length = 0,
	  .flags = 0b11, // both ACK and SYN flag set
	  .unused = 0
  };
  memcpy(sending_pkt.payload, buffer, MSS); // copy the buffer
  server_seq++; // increment sequence number after transmission
  print_diag(&sending_pkt, SEND);
  sendto(sockfd, &sending_pkt, sizeof(sending_pkt), 0, 
      (struct sockaddr *)&client_addr, sizeof(struct sockaddr_in));
  while(recvfrom(sockfd, &received_pkt, sizeof(received_pkt), 0,
                           (struct sockaddr*)&client_addr, &s) <= 0);
  print_diag(&received_pkt, RECV);               
  fprintf(stderr, "handshake complete - server\n");
  exit(1);
  
  // phase 3: receive a packet from client
  // listening merged with step 2
  // 2. Handshake complete; implement data transmission logic


  // Listen loop
  // within the sending window (20240 bytes, send all data), send all data
  // there are two channels so server sends simultaneously with the client
  // all transmitted packets must go to sndbuf and received packets must go to rcvbuf 
  // before processing to ensure FIFO delivery (CS 134)
  // retransmit only when 1) there are 3 duplicate ACKs or 2) 1 second of no ACK
  while (1) {
    start_clock = ack_recvd ? clock() : start_clock;  // reset start iff ack is received
    // Receive from socket
    bytes_recvd = recvfrom(sockfd, &buffer, sizeof(buffer), 0,
                               (struct sockaddr *)&client_addr, &s);
    if (bytes_recvd <= 0 && !client_connected)
      continue;
    
    // Data available to write
    if (bytes_recvd > 0) {
      write(STDOUT_FILENO, buffer, bytes_recvd);
    }

    // Read from stdin
    bytes_read = read(STDIN_FILENO, &buffer, sizeof(buffer));

    // Data available to send from stdin
    if (bytes_read > 0) {
      sendto(sockfd, &buffer, bytes_read, 0, (struct sockaddr *)&client_addr,
             sizeof(struct sockaddr_in));
    }
    end_clock = clock(); 
    
    // at the end of each loop, check if there are any ACKs
    // if there was none for 1s, retransmit packet with lowest sequence number
    if( ((double)(end_clock-start_clock)/CLOCKS_PER_SEC) >= 1.0) 
    {
      if (!ack_recvd)
      {
        // resend the packet with lowest sequence number
      }
    } 
  }

  return 0;
}
