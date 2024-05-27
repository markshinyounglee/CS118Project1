import socket
import time
import random
import fcntl
import os
import threading
import subprocess


class ProcessRunner(threading.Thread):
    def __init__(self, cmd, data, size):
        super().__init__()
        self.cmd = cmd
        self.process = None
        self.stdout = b""
        self.lock = threading.Lock()
        self.data = data
        self.size = size

    def run(self):
        self.process = subprocess.Popen(self.cmd.split(
        ), stdin=self.data, stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=0)
        if not self.process.stdout:
            self.process.terminate()
            return
        
        while True:
            output = self.process.stdout.read(1024)
            if output:
                with self.lock:
                    self.stdout += output
                    if len(self.stdout) >= self.size:
                        break
            else:
                break

    @staticmethod
    def run_two_until_size_or_timeout(runner1, runner2, size, timeout):
        runner1.start()
        time.sleep(0.1)
        runner2.start()
        start_time = time.time()
        while time.time() - start_time < timeout:
            with runner1.lock, runner2.lock:
                if len(runner1.stdout) >= size and len(runner2.stdout) >= size:
                    break
            time.sleep(0.1)

        runner1.process.terminate()
        runner2.process.terminate()
        runner1.join()
        runner2.join()



def byte_diff(bytes1, bytes2):
    min_len = min(len(bytes1), len(bytes2))
    max_len = max(len(bytes1), len(bytes2))
    differing_bytes = sum(b1 != b2 for b1, b2 in zip(
        bytes1[:min_len], bytes2[:min_len]))
    differing_bytes += (max_len - min_len)
    return round((differing_bytes / max_len if max_len > 0 else 1) * 100, 2)


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
                continue
            if random.random() > loss_rate:
                time.sleep(delay)
                s.sendto(data, ('localhost', server_port))
        except BlockingIOError:
            pass

        try:
            data, _ = s.recvfrom(4096)
            if len(data) > 1036 or len(data) <= 0:
                continue
            if random.random() > loss_rate:
                time.sleep(delay)
                if c_addr:
                    c.sendto(data, c_addr)
        except BlockingIOError:
            pass


# Usage
# threading.Thread(target=proxy, args=(8081, 8080, 0.05, 0.1)).start()
