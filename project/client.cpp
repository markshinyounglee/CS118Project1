#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <list>
#include <cstring>
#include <climits>
#include "diagnostics.h" // delete after use

#define MAX_WINDOW 20240
#define MAX_DELAY_HOLD 3
#define TIMEOUT_INTVL 1

uint32_t client_packet_num = 0; // delete after use
volatile sig_atomic_t retransmit_flag = false; // used for retransmission timer

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
  uint32_t len;  // check the length of buffer to be less than MAX_WINDOW
} ntbuf;

ntbuf sndbuf; // restricted to 20240 bytes
ntbuf rcvbuf; // resizable

int tempcounter = 0;

void retransmit_packet(int);
void print_rcvbuf();
void print_sndbuf();
int main(int argc, char **argv) {
  if (argc < 3) {
    //# fprintf(stderr, "Usage: client <hostname> <port> \n");
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
  char *addr = strcmp(argv[1], "localhost") == 0 ? strdup("127.0.0.1") : strdup(argv[1]);
  server_addr.sin_addr.s_addr = inet_addr(addr);
  // Set sending port
  int PORT = atoi(argv[2]);
  server_addr.sin_port = htons(PORT); // Big endian
  socklen_t s = sizeof(struct sockaddr_in);
  char buffer[MSS] = {0}; // use memset(buffer, 0, (MSS) * sizeof(char)) if this doesn't work

  // 1. Perform handshake
  srand(time(NULL));
  uint32_t client_seq = 100; // rand() % (INT_MAX/4); // 100; // return random number for packet
  // max seq number is INT_MAX/2
  uint32_t ack = 0;
  bool ack_recvd = false;
  packet received_pkt = {0};  // pkt variable we use
  packet sending_pkt = {0};
  int bytes_read = 0;
  int bytes_recvd = 0;
  int ack_counter = 1; 
  uint32_t prev_ack = 0; // keep track of same acks
  uint32_t received_ack; // ACK received from the latest packet
  
  // Assume the socket has been set up with all other variables
  // phase 1: send a packet with 0 length and SYN flag (LSB) on
  sending_pkt = { 
    .ack = 0,
	  .seq = htonl(client_seq), 
	  .length = 0,
	  .flags = 1, // only SYNC 
	  .unused = 0,
    .packet_num = 0 // delete after use
  };
  memcpy(sending_pkt.payload, buffer, MSS); // copy the buffer element
  client_seq++; // increment
  sndbuf.bufcontent.push_back(sending_pkt); // push sending packets to buffer
  //# print_diag(&sending_pkt, SEND);
  sendto(sockfd, &sending_pkt, sizeof(sending_pkt), 0, 
      (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in)); // assume sendto always succeeds
  // phase 2: receive data from server
  while(recvfrom(sockfd, &received_pkt, sizeof(received_pkt), 0,
                           (struct sockaddr*)&server_addr, &s) <= 0);
  ack = ntohl(received_pkt.seq)+1; // handshake has zero payload
  client_packet_num = ntohl(received_pkt.packet_num) + 1; // delete after use
  //# print_diag(&received_pkt, RECV);
  for (list<packet>::iterator iter = sndbuf.bufcontent.begin(); iter != sndbuf.bufcontent.end();)
  {
    if (ntohl(iter->seq) < ack)
    {
      sndbuf.len -= ntohs(iter->length);
      iter = sndbuf.bufcontent.erase(iter);
    }
    else
    {
      ++iter;
    }
  }
  
  // phase 3: send a final packet to the server
  sending_pkt = {
    .ack = htonl(ack), // Make sure to convert to little endian
	  .seq = htonl(client_seq),
	  .length = 0,
	  .flags = 0b10, // only ACK
	  .unused = 0,
    .packet_num = htonl(client_packet_num) // delete after use
  };
  memcpy(sending_pkt.payload, buffer, MSS); // copy the buffer
  client_seq++;
  sndbuf.bufcontent.push_back(sending_pkt); // queue in buffer
  sndbuf.len += bytes_read; // increment current payload length
  //# print_diag(&sending_pkt, SEND);
  sendto(sockfd, &sending_pkt, sizeof(sending_pkt), 0, 
      (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in)); // assume sendto always succeeds
  //# fprintf(stderr, "handshake complete - client\n");
  // exit(1);

  // 2. Handshake complete. Start sending real data
  // within the sending window (20240 bytes, send all data), send all data
  // there are two channels so client sends simultaneously with the server
  // all transmitted packets must go to sndbuf and received packets must go to rcvbuf 
  // before processing to ensure FIFO delivery (CS 134)
  // retransmit only when 1) there are 3 duplicate ACKs or 2) 1 second of no ACK
  // Listen loop
  while (1) {
    // part 1. receive logic
    // Receive from socket
    alarm(TIMEOUT_INTVL);  // set alarm for 1s timeout
    while((bytes_recvd = recvfrom(sockfd, &received_pkt, sizeof(received_pkt), 0,
                               (struct sockaddr *)&server_addr, &s)) <= 0) // retransmission logic
    {
      RETRANSMIT:
        signal(SIGALRM, retransmit_packet);
        if (retransmit_flag == true)
        {
          if (!sndbuf.bufcontent.empty())
          {
            // resend the packet with lowest sequence number
            sending_pkt = sndbuf.bufcontent.front();
            // modify the ACK to reflect what we want 
            sending_pkt.ack = ntohl(ack);
            //# fprintf(stderr, "Retransmitted: 1 second timeout -- client\n");
            //# print_diag(&sending_pkt, SEND);
            sendto(sockfd, &sending_pkt, sizeof(sending_pkt), 0, (struct sockaddr *)&server_addr,
                sizeof(struct sockaddr_in));
            retransmit_flag = false;
            alarm(TIMEOUT_INTVL); // reset the alarm
          }
          else
          {
            // we know there is nothing more to retransmit (i.e., the recipient received all the packets)
            // in this case, keep waiting because we might still be waiting for more packets
            //# if(tempcounter < 7)
            //# {
            //#   fprintf(stderr, "timeout but nothing to send\n");
            //#   tempcounter++;
            //# }    
          }
        }
    }
    if(bytes_recvd > 0)
    {
      //# print_diag(&received_pkt, RECV);
      ack_recvd = (received_pkt.flags >> 1) & 1;
      // if the ACK flag is set, scan the sndbuf and remove all packets with sequence number less than ACK number
      if (ack_recvd) // if the ACK flag is set
      {
        alarm(0); // cancel pending alarm
        received_ack = ntohl(received_pkt.ack); // dealing with received packets

        // if there are 3 duplicate acks, you should retransmit
        // if 3 duplicate ACKs, resend the packet with the lowest sequence number
        if (received_ack == prev_ack)
        {
          ack_counter++;
          //# fprintf(stderr, "ack_counter: %d\n", ack_counter);
          //# print_rcvbuf();
        }
        else
        {
          ack_counter = 1;
          //# fprintf(stderr, "ack_counter: %d\n", ack_counter);
          //# print_rcvbuf();
        }
        if (ack_counter >= MAX_DELAY_HOLD) 
        {
          sending_pkt = sndbuf.bufcontent.front();
          sending_pkt.ack = htonl(ack); // modify the ACK for retransmission
          //# fprintf(stderr, "Retransmitted: 3 duplicate ACKs -- client\n");
          //# print_diag(&sending_pkt, SEND);
          sendto(sockfd, &sending_pkt, sizeof(sending_pkt), 0, (struct sockaddr *)&server_addr,
                sizeof(struct sockaddr_in));
          ack_counter = 0;
        }
        prev_ack = received_ack;

        // do linear scan for send buffer
        for (list<packet>::iterator iter = sndbuf.bufcontent.begin(); iter != sndbuf.bufcontent.end(); )
        {
          if(ntohl(iter->seq) < received_ack)
          {
            sndbuf.len -= ntohs(iter->length);
            iter = sndbuf.bufcontent.erase(iter);
            //# fprintf(stderr, "send buffer popped -- client\n");
            //# fprintf(stderr, "remaining length: %d\n", sndbuf.len);
            //# print_sndbuf();
          }
          else
          {
            ++iter;
          }
        }

        // place the packet in the receiving buffer
        // condition: rcvbuf is not full and there are no duplicates
        // because of sliding window
        if(MAX_WINDOW - rcvbuf.len >= ntohs(received_pkt.length))
        {
          bool isdup = false;
          for (list<packet>::iterator iter = rcvbuf.bufcontent.begin(); iter != rcvbuf.bufcontent.end(); ++iter)
          {
            if (iter->seq == received_pkt.seq)
            {
              isdup = true;
              break;
            }
          }
          if(!isdup)
          {
            rcvbuf.bufcontent.push_back(received_pkt);
            rcvbuf.len += ntohs(received_pkt.length);
          }
        }
      }
      else // if ACK is not set
      {
        goto RETRANSMIT;
      }
    }
    
    // do a linear scan in the receiving buffer starting with the next SEQ number
    // do this concurrently with writes and reads
    for (list<packet>::iterator iter = rcvbuf.bufcontent.begin(); iter != rcvbuf.bufcontent.end(); )
    {
      // writebuflen = 0;
      if(ack == ntohl(iter->seq)) // if what we found is what we want // we increment ack only when we pop
      {
        uint16_t payload_size = ntohs(iter->length);
        ack += payload_size; // increment by the payload length
        // instead of putting things in writebuf, just write to STDOUT directly
        write(STDOUT_FILENO, iter->payload, payload_size);
        //# fprintf(stderr, "ack number is now %d -- client\n", ack);
        rcvbuf.len -= payload_size;
        iter = rcvbuf.bufcontent.erase(iter);
      }
      else if(ack > ntohl(iter->seq)) 
      // if ACK we are looking for is greater than what we have in the receiving buffer
      // that packet must have already been received; in other words, this is a duplicate packet
      {
        uint16_t payload_size = ntohs(iter->length);
        rcvbuf.len -= payload_size;
        iter = rcvbuf.bufcontent.erase(iter);
      }
      else
      {
        ++iter;
      }
    }


    // part 2. send logic
    // after the linear scan, set the next ACK number
    // we want the next in-order packet number
    // The logic is already handled previously as we increment ack

    // Read from stdin
    // the amount depends on how much space there is left in sndbuf
    uint32_t read_size = 0;
    if (MSS > (MAX_WINDOW - sndbuf.len))
    {
      read_size = MAX_WINDOW - sndbuf.len;
    }
    else
    {
      read_size = MSS;
    }
    memset(buffer, 0, MSS);
    bytes_read = read(STDIN_FILENO, &buffer, read_size);

    // Data available to send from stdin
    // Keep sending as long as the sending buffer has space
    if (bytes_read > 0 && sndbuf.len <= MAX_WINDOW) {
      // 1. place it in a new packet's payload
      sending_pkt = { 
        .ack = htonl(ack),
        .seq = htonl(client_seq), 
        .length = htons(bytes_read),
        .flags = 0b10, // only ACK flag set
        .unused = 0
      };
      memcpy(sending_pkt.payload, buffer, MSS); // copy the buffer element
      client_seq += bytes_read; // increment by payload length
      // 2. send the packet AND keep it in the sending buffer
      sndbuf.bufcontent.push_back(sending_pkt);
      sndbuf.len += bytes_read;
      //# print_diag(&sending_pkt, SEND);
      sendto(sockfd, &sending_pkt, sizeof(sending_pkt), 0, (struct sockaddr *)&server_addr,
             sizeof(struct sockaddr_in));
    }
    else if( (bytes_read == 0 && !rcvbuf.bufcontent.empty()) || sndbuf.len >= MAX_WINDOW) // EOF or max window reached
    {
      // send a dedicated ACK packet
      //# fprintf(stderr, "send dedicated ACK packet -- client\n");
      sending_pkt = { 
        .ack = htonl(ack),
        .seq = 0, 
        .length = 0,
        .flags = 0b10, // only ACK 
        .unused = 0
      };
      memset(sending_pkt.payload, 0, MSS); // reset to 0
      // directly send ACK packet and don't put in the sendbuffer
      //# print_diag(&sending_pkt, SEND);
      sendto(sockfd, &sending_pkt, sizeof(sending_pkt), 0, (struct sockaddr *)&server_addr,
             sizeof(struct sockaddr_in));
    }
  }

  close(sockfd);
  return 0;
}


void retransmit_packet(int sig)
{
  retransmit_flag = true;
}
void print_rcvbuf()
{
  fprintf(stderr, "client receiving buffer: ");
  for (list<packet>::iterator iter = rcvbuf.bufcontent.begin(); iter != rcvbuf.bufcontent.end(); ++iter)
  {
    fprintf(stderr, "%d ", ntohl(iter->seq));
  }
  fprintf(stderr, "\n");
}
void print_sndbuf()
{
  fprintf(stderr, "client sending buffer: ");
  for (list<packet>::iterator iter = sndbuf.bufcontent.begin(); iter != sndbuf.bufcontent.end(); ++iter)
  {
    fprintf(stderr, "%d -- ", ntohl(iter->seq));
  }
  fprintf(stderr, "\n");
}
