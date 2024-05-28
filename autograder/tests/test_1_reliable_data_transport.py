import unittest
import multiprocessing

import test_0_compilation

from random import randbytes
from gradescope_utils.autograder_utils.decorators import weight, number, hide_errors, partial_credit
from utils import proxy, byte_diff, ProcessRunner

DROP_RATE = 0.05
REORDER_RATE = 0.5
start_port = 8080

class TestRDT(unittest.TestCase):

    def server_test(self, size, timeout, use_rd, set_fail=False):
        global start_port
        start_port += 1
        server_port = start_port
        start_port += 1
        client_port  = start_port
        # Start proxy
        if use_rd:
            p_thread = multiprocessing.Process(target=proxy, args=(client_port, server_port, DROP_RATE, REORDER_RATE))
        else:
            p_thread = multiprocessing.Process(target=proxy, args=(client_port, server_port, 0, 0))
        p_thread.start()
        
        # Generate random file to send
        file = randbytes(size)

        # Create student server and our client
        server_runner = ProcessRunner(f'runuser -u student -- {test_0_compilation.dir}/server 0 {server_port}', file)
        client_runner = ProcessRunner(f'/autograder/source/src/client 0 localhost {client_port}', file)
        
        # Run both processes and stop when both have outputted the right amount
        # of bytes (or on timeout)
        ProcessRunner.run_two_until_size_or_timeout(server_runner, client_runner, size, timeout)
        p_thread.terminate()

        # Compare both the server and client output to original file
        fail = False
        if file != server_runner.stdout:
            fail = True
            print("Your server didn't receive data from our client correctly.")
            print(f"We sent {size} bytes and your server received {len(server_runner.stdout)} bytes ", end='')
            print(f'with a percent difference of {byte_diff(file, server_runner.stdout)}%')

        if file != client_runner.stdout:
            fail = True
            print("Your server didn't send data back to our client correctly.")
            print(f"We inputted {size} bytes in your server and we received {len(client_runner.stdout)} bytes ", end='')
            print(f'with a percent difference of {byte_diff(file, client_runner.stdout)}%')

        if fail: self.fail()

    def client_test(self, size, timeout, use_rd, set_fail=False):
        global start_port
        start_port += 1
        server_port = start_port
        start_port += 1
        client_port  = start_port
        # Start proxy
        if use_rd:
            p_thread = multiprocessing.Process(target=proxy, args=(client_port, server_port, DROP_RATE, REORDER_RATE))
        else:
            p_thread = multiprocessing.Process(target=proxy, args=(client_port, server_port, 0, 0))
        p_thread.start()
        
        # Generate random file to send
        file = randbytes(size)

        # Create student server and our client
        client_runner = ProcessRunner(f'runuser -u student -- {test_0_compilation.dir}/client 0 localhost {client_port}', file)
        server_runner = ProcessRunner(f'/autograder/source/src/server 0 {server_port}', file)
        
        # Run both processes and stop when both have outputted the right amount
        # of bytes (or on timeout)
        ProcessRunner.run_two_until_size_or_timeout(server_runner, client_runner, size, timeout)
        p_thread.terminate()

        # Compare both the server and client output to original file
        fail = False
        if file != server_runner.stdout:
            fail = True
            print("Your client didn't send data back to our server correctly.")
            print(f"We inputted {size} bytes in your client and we received {len(server_runner.stdout)} bytes ", end='')
            print(f'with a percent difference of {byte_diff(file, server_runner.stdout)}%')

        if file != client_runner.stdout:
            fail = True
            print("Your client didn't receive data from our server correctly.")
            print(f"We sent {size} bytes and your client received {len(client_runner.stdout)} bytes ", end='')
            print(f'with a percent difference of {byte_diff(file, client_runner.stdout)}%')

        if fail: self.fail()

    @weight(3)
    @number(1.1)
    @hide_errors()
    def test_server_small_nodropdelay(self):
        """Reliable Data Transport (Server): Small file (2 KB), no drop or reordering"""
        if test_0_compilation.failed: self.fail()

        FILE_SIZE = 2000
        TIMEOUT = 5

        self.server_test(FILE_SIZE, TIMEOUT, False)

    @weight(7)
    @number(1.2)
    @hide_errors()
    def test_server_large_nodropdelay(self):
        """Reliable Data Transport (Server): Large file (2 MB), no drop or reordering"""
        if test_0_compilation.failed: self.fail()

        FILE_SIZE = 2000000
        TIMEOUT = 10

        self.server_test(FILE_SIZE, TIMEOUT, False)

    @weight(15)
    @number(1.3)
    @hide_errors()
    def test_server_medium_dropdelay(self):
        """Reliable Data Transport (Server): Medium file (200 KB), drop rate of 5%, reordering"""
        if test_0_compilation.failed: self.fail()

        FILE_SIZE = 200000
        TIMEOUT = 60

        self.server_test(FILE_SIZE, TIMEOUT, True)

    @weight(3)
    @number(1.4)
    @hide_errors()
    def test_client_small_nodropdelay(self):
        """Reliable Data Transport (Client): Small file (2 KB), no drop or reordering"""
        if test_0_compilation.failed: self.fail()

        FILE_SIZE = 2000
        TIMEOUT = 5

        self.client_test(FILE_SIZE, TIMEOUT, False)

    @weight(7)
    @number(1.5)
    @hide_errors()
    def test_client_large_nodropdelay(self):
        """Reliable Data Transport (Client): Large file (2 MB), no drop or reordering"""
        if test_0_compilation.failed: self.fail()

        FILE_SIZE = 2000000
        TIMEOUT = 10

        self.client_test(FILE_SIZE, TIMEOUT, False)

    @weight(15)
    @number(1.6)
    @hide_errors()
    def test_client_medium_dropdelay(self):
        """Reliable Data Transport (Client): Medium file (200 KB), drop rate of 5%, reordering"""
        if test_0_compilation.failed: self.fail()

        FILE_SIZE = 200000
        TIMEOUT = 60

        self.client_test(FILE_SIZE, TIMEOUT, True)

        
