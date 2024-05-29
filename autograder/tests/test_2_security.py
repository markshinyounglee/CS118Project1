import unittest
import time
import socket
import os
import fcntl

import test_0_compilation

from gradescope_utils.autograder_utils.decorators import weight, number, hide_errors
from utils import ProcessRunner

start_port = 8180
s_ref = "/autograder/submission/"
r_ref = "/autograder/source/src/"
ca_pub = "/autograder/source/keys/ca_pub_key.bin"
bad_ca_pub = "/autograder/source/keys/ca_pub_key_2.bin"
priv = "/autograder/source/keys/priv_key.bin"
bad_priv = "/autograder/source/keys/priv_key_2.bin"
cert = "/autograder/source/keys/cert.bin"


class TestSecurity(unittest.TestCase):
    def make_test(self, name, set_state, bad_priv_key=False, bad_pub=False):
        TIMEOUT = 3

        global start_port
        start_port += 1
        server_port = start_port
        start_port += 1
        client_port = start_port

        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        c = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        fcntl.fcntl(s, fcntl.F_SETFL, os.O_NONBLOCK)
        fcntl.fcntl(c, fcntl.F_SETFL, os.O_NONBLOCK)
        c.bind(('localhost', client_port))

        server_runner = ProcessRunner(
            f'{r_ref}/server 1 {server_port} {bad_priv_key if bad_priv else priv} {cert} 0', b'test'*2000, name + "_refserver.out")
        client_runner = ProcessRunner(
            f'{s_ref}/client 1 localhost {client_port} {bad_ca_pub if bad_pub else ca_pub}', b'test'*2000, name + "_yourclient.out")

        server_runner.run()
        time.sleep(0.1)
        client_runner.run()

        start_time = time.time()
        state = 0
        failed = True
        while time.time() - start_time < TIMEOUT:
            if state == 0:
                try:
                    packet, _ = c.recvfrom(2000)
                except BlockingIOError:
                    continue

                if packet:
                    s.sendto(packet, ('localhost', server_port))
                    state = 1
            elif state == 1:
                try:
                    packet, _ = s.recvfrom(2000)
                except BlockingIOError:
                    continue

                if packet:
                    if set_state == 1:
                        failed = False
                        break
                    else:
                        continue
                if server_runner.process and server_runner.process.poll():
                    print(
                        "Your client failed to generate an acceptable `client_hello`.")
                    self.fail()

        if failed:
            print("Your client failed to send a `client_hello`.")
            self.fail()

    @weight(5)
    @number(2.1)
    @hide_errors()
    def test_client_hello(self):
        """Security: Sends correct `client_hello`"""
        if test_0_compilation.failed:
            self.fail()
        name = self.test_client_hello.__name__
