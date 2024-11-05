#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include <list>


#define MAX_WINDOW 20240
#define DELAY_RETRANSMIT 1
#define MAX_DELAY_HOLD 3
#define MSS 1012 // MSS = Maximum Segment Size (aka max length)
typedef struct {
	uint32_t ack;
	uint32_t seq;
	uint16_t length;
	uint8_t flags;
	uint8_t unused;
	uint8_t payload[MSS];
} packet;


using namespace std;

typedef struct { // sliding window
  list<packet> bufcontent; // payload length should be 20240
  unsigned int len;  // check the length of buffer to be less than MAX_WINDOW
} ntbuf;

ntbuf sndbuf; // restricted to 20240 bytes
ntbuf rcvbuf; // resizable

clock_t start = 0, end = 0; // keep track of time elapsed

packet* makePacket(uint32_t, uint32_t, uint16_t, 
                  uint8_t, uint8_t, uint8_t, uint8_t*);
// for sending buffer, use resizable array (linked list)
// while ensuring that the sum of payloads stay within 20240 bytes
// for receiving buffer, receive as much as you want

int main(int argc, char **argv) {
  if (argc < 3) {
    fprintf(stderr, "Usage: client <hostname> <port> \n");
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

  /* Construct server address */
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET; // use IPv4
  // Only supports localhost as a hostname, but that's all we'll test on
  char *addr = strcmp(argv[1], "localhost") == 0 ? "127.0.0.1" : argv[1];
  server_addr.sin_addr.s_addr = inet_addr(addr);
  // Set sending port
  int PORT = atoi(argv[2]);
  server_addr.sin_port = htons(PORT); // Big endian
  socklen_t s = sizeof(struct sockaddr_in);
  char buffer[MSS] = {0}; // use memset(buffer, 0, (MSS) * sizeof(char)) if this doesn't work

  // 1. Perform handshake
  srand(time(NULL));
  uint32_t client_seq = 100; // rand(); // pick random number later on
  uint32_t ack = 0;
  bool ack_recvd = false;
  packet received_pkt = {0};  // pkt variable we use
  packet sending_pkt = {0};
  int bytes_read = 0;
  int bytes_recvd = 0;
  
  // Assume the socket has been set up with all other variables
  // phase 1: send a packet with 0 length and SYN flag (LSB) on
  sending_pkt = { 
    .ack = 0,
	  .seq = client_seq, 
	  .length = 0,
	  .flags = 1, // only SYNC 
	  .unused = 0,
	  .payload = buffer
  };
  client_seq++; // increment
  while (sendto(sockfd, &sending_pkt, sizeof(sending_pkt), 0, 
      (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in)) != -1);
  // phase 2: receive data from server
  while(recvfrom(sockfd, &received_pkt, sizeof(received_pkt), 0,
                           (struct sockaddr*), &server_addr, &s) <= 0);
  // phase 3: send a final packet to the server
  // Read from stdin
  bytes_read = read(STDIN_FILENO, &buffer, sizeof(buffer));
  sending_pkt = {
    .ack = ntohl(received_pkt.seq) + 1, // Make sure to convert to little endian
	  .seq = client_seq,
	  .length = bytes_read,
	  .flags = 0b10, // only ACK
	  .unused = 0,
	  .payload = buffer
  };
  client_seq++;
  sndbuf.bufcontent.push_back(sending_pkt); // queue in buffer
  sndbuf.len += bytes_read; // increment current payload length
  while (sendto(sockfd, &sending_pkt, sizeof(sending_pkt), 0, 
      (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in)) != -1);
  
  // 2. Handshake complete. Start sending real data
  // within the sending window (20240 bytes, send all data), send all data
  // there are two channels so client sends simultaneously with the server
  // all transmitted packets must go to sndbuf and received packets must go to rcvbuf 
  // before processing to ensure FIFO delivery (CS 134)
  // retransmit only when 1) there are 3 duplicate ACKs or 2) 1 second of no ACK
  // Listen loop
  while (1) {
    start = ack_recvd ? clock() : start;  // reset start iff ack is received
    // Receive from socket
    bytes_recvd = recvfrom(sockfd, &buffer, sizeof(buffer), 0,
                               (struct sockaddr *)&server_addr, &s);

    // Data available to write
    if (bytes_recvd > 0) {
      write(STDOUT_FILENO, buffer, bytes_recvd);
    }

    // Read from stdin
    bytes_read = read(STDIN_FILENO, &buffer, sizeof(buffer));

    // Data available to send from stdin
    if (bytes_read > 0) {
      sendto(sockfd, &buffer, bytes_read, 0, (struct sockaddr *)&server_addr,
             sizeof(struct sockaddr_in));
    }
    end = clock(); 
    
    // at the end of each loop, check if there are any ACKs
    // if there was none for 1s, retransmit packet with lowest sequence number
    if( ((double)(end-start)/CLOCKS_PER_SEC) >= 1.0) 
    {
      if(!ack_recvd)
      {
        // resend the packet with lowest sequence number
      }
    } 
  }

  return 0;
}


// stub code -- JUST IN CASE BUT PREFERRED NOT TO USE
// try not to use dynamic memory allocation...
packet* makePacket(uint32_t, uint32_t, uint16_t, 
                  uint8_t, uint8_t, uint8_t, uint8_t*)
{
  pkt = (packet*)malloc(sizeof(packet));


  return pkt;
}
