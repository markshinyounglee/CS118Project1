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
#include <climits>
#include "diagnostics.h" // delete after use

#define MAX_WINDOW 20240
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

void print_rcvbuf();
void print_sndbuf();
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
  uint32_t server_seq = 300; // rand() % (INT_MAX/4); // 300; // return random number for packet
  // max seq number is INT_MAX/2
  uint32_t ack;
  bool ack_recvd = false;
  packet received_pkt = {0};  // pkt variable we use
  packet sending_pkt = {0};
  int bytes_read = 0;
  int bytes_recvd = 0;
  int ack_counter = 1; 
  uint32_t prev_ack = 0; // keep track of same acks
  uint32_t received_ack; // ACK received from the latest packet
  
  
  // Assume the socket has been set up with all other variables
  // phase 1: receive SYN packet from client
  while(1)
  {
    bytes_recvd = recvfrom(sockfd, &received_pkt, sizeof(received_pkt), 0,
                           (struct sockaddr*)&client_addr, &s);
    if (bytes_recvd <= 0 && !client_connected)
      continue;
    client_connected = 1; // At this point, the client has connected and sent data
    ack = ntohl(received_pkt.seq)+1;
    print_diag(&received_pkt, RECV);
    break;
  }
  // phase 2:
  // after receiving data packet from client, send the ACK packet
  // respond with 0 length payload
  sending_pkt = {
    .ack = htonl(ack), // Make sure to convert to little endian
    .seq = htonl(server_seq),
    .length = 0,
	  .flags = 0b11, // both ACK and SYN flag set
	  .unused = 0
  };
  memcpy(sending_pkt.payload, buffer, MSS); // copy the buffer
  server_seq++; // increment sequence number after transmission
  sndbuf.bufcontent.push_back(sending_pkt);
  print_diag(&sending_pkt, SEND);
  sendto(sockfd, &sending_pkt, sizeof(sending_pkt), 0, 
      (struct sockaddr *)&client_addr, sizeof(struct sockaddr_in));

  while(1)
  {
    bytes_recvd = recvfrom(sockfd, &received_pkt, sizeof(received_pkt), 0,
                           (struct sockaddr*)&client_addr, &s);
    if (bytes_recvd <= 0)
      continue;
    uint16_t payload_size = ntohs(received_pkt.length);
    if (payload_size >= 1) {
      ack = ntohl(received_pkt.seq) + payload_size;
      write(STDOUT_FILENO, received_pkt.payload, payload_size);
      fprintf(stderr, "case 1 -- server\n");
    }
    else
    {
      ack = ntohl(received_pkt.seq) + 1;
      fprintf(stderr, "case 2 -- server\n");
    }
    print_diag(&received_pkt, RECV);

    // remove all packets smaller than ACK in sndbuf
    for (list<packet>::iterator iter = sndbuf.bufcontent.begin(); iter != sndbuf.bufcontent.end(); )
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
    break;
  }
  
  /*
  while(recvfrom(sockfd, &received_pkt, sizeof(received_pkt), 0,
                           (struct sockaddr*)&client_addr, &s) <= 0);
  ack = ntohl(received_pkt.seq)+1;
  print_diag(&received_pkt, RECV);   
  */      
       
  fprintf(stderr, "handshake complete - server\n");
  // exit(1);
  
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
    
    // part 1. receive logic
    // Receive from socket
    bytes_recvd = recvfrom(sockfd, &received_pkt, sizeof(received_pkt), 0,
                               (struct sockaddr *)&client_addr, &s);
    if(bytes_recvd > 0)
    {
      print_diag(&received_pkt, RECV);
      ack_recvd = (received_pkt.flags >> 1) & 1;
      // if the ACK flag is set, scan the sndbuf and remove all packets with sequence number less than ACK number
      if (ack_recvd) // if the ACK flag is set
      {
        received_ack = ntohl(received_pkt.ack); // dealing with received packets

        // if there are 3 duplicate acks, you should retransmit
        // if 3 duplicate ACKs, resend the packet with the lowest sequence number
        if (received_ack == prev_ack)
        {
          ack_counter++;
          fprintf(stderr, "ack_counter: %d\n", ack_counter);
          print_rcvbuf();
        }
        else
        {
          ack_counter = 1;
          fprintf(stderr, "ack_counter: %d\n", ack_counter);
          print_rcvbuf();
        }
        if (ack_counter >= MAX_DELAY_HOLD) 
        {
          sending_pkt = sndbuf.bufcontent.front();
          sending_pkt.ack = htonl(ack); // modify the ACK for retransmission
          fprintf(stderr, "Retransmitted: 3 duplicate ACKs -- server\n");
          print_diag(&sending_pkt, SEND);
          sendto(sockfd, &sending_pkt, sizeof(sending_pkt), 0, (struct sockaddr *)&client_addr,
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
            fprintf(stderr, "send buffer popped -- server\n");
            fprintf(stderr, "remaining length: %d\n", sndbuf.len);
            print_sndbuf();
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
          rcvbuf.bufcontent.push_back(received_pkt);
          rcvbuf.len += ntohs(received_pkt.length);
        }
        /*
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
        */
      }
    }
    else
    {
      ack_recvd = false;
    }
    
    // do this concurrently with writes and reads
    // do a linear scan in the receiving buffer starting with the next SEQ number
    for (list<packet>::iterator iter = rcvbuf.bufcontent.begin(); iter != rcvbuf.bufcontent.end(); )
    {
      // writebuflen = 0;
      if(ack == ntohl(iter->seq)) // if what we found is what we want // we increment ack only when we pop
      {
        uint16_t payload_size = ntohs(iter->length);
        ack += payload_size; // increment by the payload length
        // instead of putting things in writebuf, just write to STDOUT directly
        write(STDOUT_FILENO, iter->payload, payload_size);
        fprintf(stderr, "ack number is now %d -- server\n", ack);
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
        .seq = htonl(server_seq), 
        .length = htons(bytes_read),
        .flags = 0b10, // only ACK flag set
        .unused = 0
      };
      memcpy(sending_pkt.payload, buffer, MSS); // copy the buffer element
      server_seq += bytes_read; // increment by payload length
      // 2. send the packet AND keep it in the sending buffer
      sndbuf.bufcontent.push_back(sending_pkt);
      sndbuf.len += bytes_read;
      print_diag(&sending_pkt, SEND);
      sendto(sockfd, &sending_pkt, sizeof(sending_pkt), 0, (struct sockaddr *)&client_addr,
             sizeof(struct sockaddr_in));
    }
    else if( (bytes_read == 0 && !rcvbuf.bufcontent.empty()) || sndbuf.len >= MAX_WINDOW) // EOF or max window reached
    {
      // send a dedicated ACK packet
      fprintf(stderr, "send dedicated ACK packet -- server\n");
      sending_pkt = { 
        .ack = htonl(ack),
        .seq = 0, 
        .length = 0,
        .flags = 0b10, // only ACK 
        .unused = 0
      };
      memset(sending_pkt.payload, 0, MSS); // reset to 0
      // directly send ACK packet and don't put in the sendbuffer
      print_diag(&sending_pkt, SEND);
      sendto(sockfd, &sending_pkt, sizeof(sending_pkt), 0, (struct sockaddr *)&client_addr,
             sizeof(struct sockaddr_in));
    }
    end_clock = clock(); 
    

    // part 3. retransmit logic
    // also incorporated in send logic
    // at the end of each loop, check if there are any ACKs
    // if there was none for 1s, retransmit packet with lowest sequence number
    if( ((double)(end_clock-start_clock)/CLOCKS_PER_SEC) >= 1.0) 
    {
      if(!ack_recvd && !sndbuf.bufcontent.empty())
      {
        // resend the packet with lowest sequence number
        sending_pkt = sndbuf.bufcontent.front();
        // modify the ACK to reflect what we want 
        sending_pkt.ack = ntohl(ack);
        fprintf(stderr, "Retransmitted: 1 second timeout -- server\n");
        print_diag(&sending_pkt, SEND);
        sendto(sockfd, &sending_pkt, sizeof(sending_pkt), 0, (struct sockaddr *)&client_addr,
             sizeof(struct sockaddr_in));
        start_clock = end_clock = clock();
      }
      else // if(ack_recvd || sndbuf.bufcontent.empty()) // reset the timer if we receive a new ACK or sending buffer is empty
      {
        start_clock = end_clock = clock();
      }
    } 
  }

  return 0;
}

void print_rcvbuf()
{
  fprintf(stderr, "server receiving buffer: ");
  for (list<packet>::iterator iter = rcvbuf.bufcontent.begin(); iter != rcvbuf.bufcontent.end(); ++iter)
  {
    fprintf(stderr, "%d -- ", ntohl(iter->seq));
  }
  fprintf(stderr, "\n");
}
void print_sndbuf()
{
  fprintf(stderr, "server sending buffer: ");
  for (list<packet>::iterator iter = sndbuf.bufcontent.begin(); iter != sndbuf.bufcontent.end(); ++iter)
  {
    fprintf(stderr, "%d -- ", ntohl(iter->seq));
  }
  fprintf(stderr, "\n");
}