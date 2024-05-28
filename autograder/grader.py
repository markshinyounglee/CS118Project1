import errno
import os
import socket
import unittest

from gradescope_utils.autograder_utils.json_test_runner import JSONTestRunner

# Prevent network connections
def prevent_network():
    try:
        import pyseccomp as seccomp
        filter = seccomp.SyscallFilter(seccomp.ALLOW)
        for network in [socket.AF_INET, socket.AF_INET6]:
            filter.add_rule(
                seccomp.ERRNO(errno.EACCES),
                "socket",
                seccomp.Arg(0, seccomp.EQ, network)
            )
        filter.load()
    except RuntimeError:
        pass


if __name__ == '__main__':
    # prevent_network()
    os.system("chmod -R o+rwx /autograder/submission")
    suite = unittest.defaultTestLoader.discover('tests')
    with open('/autograder/results/results.json', 'w') as f:
        def post_process(json):
            json['test_output_format'] = 'md'
            for test in json['tests']:
                if 'output' in test:
                    test['output'] = test['output'].replace('Test failed', '')
        runner = JSONTestRunner(
            visibility='visible', stream=f, failure_prefix="", post_processor=post_process)
        runner.run(suite)
