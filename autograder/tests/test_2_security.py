import unittest
import time
import socket
import os
import fcntl
import struct

import test_0_compilation

from gradescope_utils.autograder_utils.decorators import weight, number, hide_errors
from utils import ProcessRunner
from random import randbytes

start_port = 8180
s_ref = "/autograder/submission/"
r_ref = "/autograder/source/src/"
ca_pub = "/autograder/source/keys/ca_pub_key.bin"
bad_ca_pub = "/autograder/source/keys/ca_pub_key_2.bin"
priv = "/autograder/source/keys/priv_key.bin"
bad_priv = "/autograder/source/keys/priv_key_2.bin"
cert = "/autograder/source/keys/cert.bin"


class TestSecurity(unittest.TestCase):
    def make_test(self, name, set_state, ref_server, bad_priv_key=False, bad_pub=False):
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

        file = randbytes(500)

        if ref_server:
            server_runner = ProcessRunner(
                f'{r_ref}/server 1 {server_port} {bad_priv if bad_priv_key else priv} {cert} 0', file, name + "_refserver.out")
            client_runner = ProcessRunner(
                f'{s_ref}/client 1 localhost {client_port} {bad_ca_pub if bad_pub else ca_pub}', file, name + "_yourclient.out")
        else:
            server_runner = ProcessRunner(
                f'{s_ref}/server 1 {server_port} {bad_priv if bad_priv_key else priv} {cert} 0', file, name + "_yourserver.out")
            client_runner = ProcessRunner(
                f'{r_ref}/client 1 localhost {client_port} {bad_ca_pub if bad_pub else ca_pub}', file, name + "_refclient.out")

        server_runner.run()
        time.sleep(0.1)
        client_runner.run()

        start_time = time.time()
        global state
        state = 0
        failed = True
        global c_addr
        c_addr = None

        def process_packet(from_server, expected_type, client_err_msg, server_err_msg):
            if server_runner.process and server_runner.process.poll():
                if ref_server:
                    return client_err_msg
                else:
                    return server_err_msg

            if client_runner.process and client_runner.process.poll():
                if ref_server:
                    return client_err_msg
                else:
                    return server_err_msg
            
            try:
                if from_server:
                    packet, _ = s.recvfrom(2000)
                else:
                    global c_addr
                    packet, c_addr = c.recvfrom(2000)
            except BlockingIOError:
                return

            if packet:
                if len(packet) < 13 or struct.unpack("12xB", packet[:13])[0] != expected_type:
                    if ref_server:
                        return client_err_msg
                    else:
                        return server_err_msg

                if from_server:
                    if c_addr:
                        c.sendto(packet, c_addr)
                else:
                    s.sendto(packet, ('localhost', server_port))
                global state
                state = state + 1

        while time.time() - start_time < TIMEOUT:
            if state == set_state:
                failed = False
                break

            msg = None
            if state == 0:
                msg = process_packet(False, 1, "Your client failed to generate a `client_hello`.",
                               "The reference server encountered an unexpected error.")
            elif state == 1:
                msg = process_packet(True, 2, "Our server did not accept your `client_hello`.", 
                               "Your server failed to generate a `server_hello`.")
            elif state == 2:
                msg = process_packet(False, 16, "Your client failed to generate a `key_exchange_request`.",
                               "Your server failed to generate a `server_hello`.")
            elif state == 3:
                msg = process_packet(True, 20, "Your client failed to generate a 'key_exchange_request`.",
                               "Your server failed to generate a `finished` message.")
            elif state == 4:
                msg = process_packet(True, 255, "Your client failed to generate a 'key_exchange_request`.",
                               "Your server failed to generate a `finished` message.")

            if msg:
                print(msg)
                self.fail()

        return not failed

    @weight(5)
    @number(2.1)
    @hide_errors()
    def test_client_hello(self):
        """Security: Sends correct `client_hello`"""
        if test_0_compilation.failed:
            self.fail()
        name = self.test_client_hello.__name__

        if not self.make_test(name, 1, True):
            print("Your client failed to send a `client_hello`.")
            self.fail()

    @weight(5)
    @number(2.2)
    @hide_errors()
    def test_server_hello(self):
        """Security: Sends correct `server_hello`"""
        if test_0_compilation.failed:
            self.fail()
        name = self.test_server_hello.__name__

        if not self.make_test(name, 3, False):
            print("Your server failed to send a `server_hello`.")
            self.fail()

    @weight(5)
    @number(2.3)
    @hide_errors()
    def test_key_exchange_request(self):
        """Security: Sends correct `key_exchange_request`"""
        if test_0_compilation.failed:
            self.fail()
        name = self.test_key_exchange_request.__name__

        if not self.make_test(name, 3, True):
            print("Your client failed to send a `key_exchange_request`.")
            self.fail()

    @weight(5)
    @number(2.4)
    @hide_errors()
    def test_finished_message(self):
        """Security: Sends correct `finished` message"""
        if test_0_compilation.failed:
            self.fail()
        name = self.test_finished_message.__name__

        if not self.make_test(name, 4, False):
            print("Your server failed to send a `finished` message.")
            self.fail()
