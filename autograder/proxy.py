import socket
import time
import random
import fcntl
import os

def proxy(client_port, server_port, loss_rate, delay):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    c = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    fcntl.fcntl(s, fcntl.F_SETFL, os.O_NONBLOCK)
    fcntl.fcntl(c, fcntl.F_SETFL, os.O_NONBLOCK)
    c.bind(('localhost', client_port))
    c_addr = None
    while True:
        try:
            data, c_addr = c.recvfrom(4096)
            if len(data) > 1036 or len(data) <= 0:
                raise Exception('Too large') 
            if random.random() > loss_rate:
                time.sleep(delay)
                s.sendto(data, ('localhost', server_port))
        except BlockingIOError:
            pass

        try:
            data, _ = s.recvfrom(4096)
            if len(data) > 1036 or len(data) <= 0:
                raise Exception('Too large') 
            if random.random() > loss_rate:
                time.sleep(delay)
                if c_addr: c.sendto(data, c_addr)
        except BlockingIOError:
            pass

# Usage
# threading.Thread(target=proxy, args=(8081, 8080, 0.05, 0.1)).start()
    