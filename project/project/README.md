# CS 118 Fall 24 Project 0

This repository contains a reference solution and autograder for [CS 118's Fall 24 Project
0](https://docs.google.com/document/d/1O6IuX39E4PoMvQ9uP98AWayqCgmnoBUoRfKCUZboKwg).


Since I was implementing this project in Linux, I had to give 
sudo access to my username to be able to run docker without sudo command. I did the following:

```shell
sudo groupadd docker
sudo usermod -aG docker $USER
newgrp docker
newgrp docker
sudo reboot # reboot the Linux machine to update the change
```
which can also be seen at: https://docs.docker.com/engine/install/linux-postinstall/


Some of the major challenges I experienced was 
1) retransmission logic
2) sending dedicated ACK packet if there is nothing to send
3) termination condition so that no packet goes missing

I had to use ACK to pop the sending buffer. When there was ACK,
I removed all the packets in the sending buffer that had 
sequence numbers less than ACK value.

I maintained two buffers--sending and receiving--for both client and server. I used list from C++ STL for convenience.
I could have also used deque since the sliding window buffer for the sending packets is FIFO.

If the ACK asks for a higher sequence value and the sending buffer is empty,
(that is, cannot send any more values)
then we know that we have reached the end of the program. That is when we can stop sending new packets

After the handshake step, the client and server share the same logic in the while loop since both are symmetrical.

When we copy the while loop over, three things to keep in mind:
- for all the debugging information, change "server" (server.c) <-> "client" (client.c)
- change sequence number variable: server_seq (server.c) <-> client.seq (client.c)
- change target address variable: client_addr (server.c) <-> server_addr (client.c)




