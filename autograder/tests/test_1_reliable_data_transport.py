import unittest
import multiprocessing

import test_0_compilation

from random import randbytes, randint, choice
from string import ascii_letters
from gradescope_utils.autograder_utils.decorators import weight, number, hide_errors
from utils import proxy, byte_diff, ProcessRunner

DROP_RATE = 0.1
REORDER_RATE = 1
start_port = 8080

class TestRDT(unittest.TestCase):

    def make_test(self, size: int, timeout: int, use_ascii: bool, ref_client: bool, ref_server: bool, drop_rate: float, reorder_rate: float, name: str) -> None:
        global start_port
        start_port += 1
        server_port = start_port
        start_port += 1
        client_port  = start_port

        # Start proxy
        p_thread = multiprocessing.Process(target=proxy, args=(client_port, server_port, drop_rate, reorder_rate))
        p_thread.start()
        
        # Generate random file to send
        if use_ascii:
            file = b''.join([choice(ascii_letters).encode() for _ in range(size)])
        else:
            file = randbytes(size)

        # Create server and client
        if ref_server:
            server_runner = ProcessRunner(f'/autograder/source/src/server 0 {server_port}', file, name + "_refserver.out")
        else:
            server_runner = ProcessRunner(f'runuser -u student -- {test_0_compilation.dir}/server 0 {server_port}', file, name + "_yourserver.out")

        if ref_client:
            client_runner = ProcessRunner(f'/autograder/source/src/client 0 localhost {client_port}', file, name + "_refclient.out")
        else:
            client_runner = ProcessRunner(f'runuser -u student -- {test_0_compilation.dir}/client 0 localhost {client_port}', file, name + "_yourclient.out")
        
        # Run both processes and stop when both have outputted the right amount
        # of bytes (or on timeout)
        ProcessRunner.run_two_until_size_or_timeout(server_runner, client_runner, size, timeout)
        p_thread.terminate()

        # Compare both the server and client output to original file
        fail = False
        if file != server_runner.stdout:
            fail = True

            if not ref_server and not ref_client:
                print("Your server didn't produce the expected result.")
                print(f"We inputted {size} bytes in your client and we received {len(server_runner.stdout)} bytes ", end='')
            elif ref_server:
                print("Your client didn't send data back to our server correctly.")
                print(f"We inputted {size} bytes in your client and we received {len(server_runner.stdout)} bytes ", end='')
            else:
                print("Your server didn't receive data from our client correctly.")
                print(f"We sent {size} bytes and your server received {len(server_runner.stdout)} bytes ", end='')
            print(f'with a percent difference of {byte_diff(file, server_runner.stdout)}%')

        if file != client_runner.stdout:
            fail = True

            if not ref_server and not ref_client:
                print("Your client didn't produce the expected result.")
                print(f"We inputted {size} bytes in your server and we received {len(client_runner.stdout)} bytes ", end='')
            elif ref_client:
                print("Your server didn't send data back to our client correctly.")
                print(f"We inputted {size} bytes in your server and we received {len(client_runner.stdout)} bytes ", end='')
            else:
                print("Your client didn't receive data from our server correctly.")
                print(f"We sent {size} bytes and your client received {len(client_runner.stdout)} bytes ", end='')
            print(f'with a percent difference of {byte_diff(file, client_runner.stdout)}%')

        if fail: self.fail()

    @weight(5)
    @number(1.1)
    @hide_errors()
    def test_self_ascii(self):
        """Reliable Data Transport (Your Client <-> Your Server): Small, ASCII only file (2 KB)"""
        if test_0_compilation.failed: self.fail()

        FILE_SIZE = 2000
        TIMEOUT = 3

        self.make_test(FILE_SIZE, TIMEOUT, True, False, False, 0, 0, self.test_self_ascii.__name__)

    @weight(5)
    @number(1.2)
    @hide_errors()
    def test_self(self):
        """Reliable Data Transport (Your Client <-> Your Server): Medium file (50 KB)"""
        if test_0_compilation.failed: self.fail()

        FILE_SIZE = 50000
        TIMEOUT = 3

        self.make_test(FILE_SIZE, TIMEOUT, False, False, False, 0, 0, self.test_self.__name__)

    @weight(5)
    @number(1.3)
    @hide_errors()
    def test_client_normal(self):
        """Reliable Data Transport (Your Client <-> Reference Server): Medium file (50 KB)"""
        if test_0_compilation.failed: self.fail()

        FILE_SIZE = 50000
        TIMEOUT = 3

        self.make_test(FILE_SIZE, TIMEOUT, False, False, True, 0, 0, self.test_client_normal.__name__)

    @weight(5)
    @number(1.4)
    @hide_errors()
    def test_server_normal(self):
        """Reliable Data Transport (Reference Client <-> Your Server): Medium file (50 KB)"""
        if test_0_compilation.failed: self.fail()

        FILE_SIZE = 50000
        TIMEOUT = 3

        self.make_test(FILE_SIZE, TIMEOUT, False, True, False, 0, 0, self.test_server_normal.__name__)

    @weight(5)
    @number(1.5)
    @hide_errors()
    def test_client_drop(self):
        """Reliable Data Transport (Your Client <-> Reference Server): Medium file (50 KB), 10% drop rate"""
        if test_0_compilation.failed: self.fail()

        FILE_SIZE = 50000
        TIMEOUT = 20

        self.make_test(FILE_SIZE, TIMEOUT, False, False, True, DROP_RATE, 0, self.test_client_drop.__name__)

    @weight(5)
    @number(1.6)
    @hide_errors()
    def test_server_drop(self):
        """Reliable Data Transport (Reference Client <-> Your Server): Medium file (50 KB), 10% drop rate"""
        if test_0_compilation.failed: self.fail()

        FILE_SIZE = 50000
        TIMEOUT = 20

        self.make_test(FILE_SIZE, TIMEOUT, False, True, False, DROP_RATE, 0, self.test_server_drop.__name__)

    @weight(5)
    @number(1.7)
    @hide_errors()
    def test_client_reorder(self):
        """Reliable Data Transport (Your Client <-> Reference Server): Medium file (50 KB), reordered"""
        if test_0_compilation.failed: self.fail()

        FILE_SIZE = 50000
        TIMEOUT = 20

        self.make_test(FILE_SIZE, TIMEOUT, False, False, True, 0, REORDER_RATE, self.test_client_reorder.__name__)

    @weight(5)
    @number(1.8)
    @hide_errors()
    def test_server_reorder(self):
        """Reliable Data Transport (Reference Client <-> Your Server): Medium file (50 KB), reordered"""
        if test_0_compilation.failed: self.fail()

        FILE_SIZE = 50000
        TIMEOUT = 20

        self.make_test(FILE_SIZE, TIMEOUT, False, True, False, 0, REORDER_RATE, self.test_server_reorder.__name__)

    @weight(5)
    @number(1.9)
    @hide_errors()
    def test_client_large(self):
        """Reliable Data Transport (Your Client <-> Reference Server): Large file (1 MB)"""
        if test_0_compilation.failed: self.fail()

        FILE_SIZE = 1000000
        TIMEOUT = 10

        self.make_test(FILE_SIZE, TIMEOUT, False, False, True, 0, 0, self.test_client_large.__name__)

    @weight(5)
    @number("1.10")
    @hide_errors()
    def test_server_large(self):
        """Reliable Data Transport (Reference Client <-> Your Server): Large file (1 MB)"""
        if test_0_compilation.failed: self.fail()

        FILE_SIZE = 1000000
        TIMEOUT = 10

        self.make_test(FILE_SIZE, TIMEOUT, False, True, False, 0, 0, self.test_server_large.__name__)
