import unittest
import subprocess
import os
from gradescope_utils.autograder_utils.decorators import weight, number, hide_errors

failed = True
dir = ""

class TestCompilation(unittest.TestCase):
    @weight(0)
    @number(0)
    @hide_errors()
    def test_submitted_files(self):
        """Compilation"""

        paths_to_check = [
            "/autograder/submission/project/Makefile",
            "/autograder/submission/Makefile"
        ]
        
        makefile_dir = None
        for path in paths_to_check:
            if os.path.isfile(path):
                makefile_dir = os.path.dirname(path)
                break

        if makefile_dir is None:
            print("Makefile not found. Verify your submission has the correct files.")
            self.fail()

        os.chdir(makefile_dir)

        try:
            subprocess.run("runuser -u student -- make".split(), check=True, capture_output=True, text=True)
        except subprocess.CalledProcessError as e:
            print("We could not compile your executables. Verify that your Makefile is valid.")
            self.fail()

        if not os.path.isfile(os.path.join(makefile_dir, 'server')):
            print("We could not find your server executable. Make sure it's named `server`.")
            self.fail()

        if not os.path.isfile(os.path.join(makefile_dir, 'client')):
            print("We could not find your client executable. Make sure it's named `client`.")
            self.fail()

        global dir
        dir = makefile_dir
        global failed
        failed = False

        