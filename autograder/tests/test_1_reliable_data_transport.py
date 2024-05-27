import unittest
import subprocess
import os
import socket
from gradescope_utils.autograder_utils.decorators import weight, number, hide_errors

class TestRDT(unittest.TestCase):
    @weight(5)
    @number(1.1)
    # @hide_errors()
    def test_small_nodropdelay(self):
        """Reliable Data Transport: Small file (5 KB), no drop or delay"""
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(("0.0.0.0", 80))
        pass