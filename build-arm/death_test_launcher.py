import argparse
import subprocess
import sys

parser = argparse.ArgumentParser( description='Death test launcher' )
parser.add_argument( 'parallel_launcher', type=str)
parser.add_argument( 'process_count', type=int)
parser.add_argument( 'executable', type=str)
parser.add_argument( 'expected_return_code', type=int)
args = parser.parse_args()
run_cmd = [args.parallel_launcher, '-engine', 'ibverbs', '-n', str(args.process_count), args.executable]
print("Death test launcher command:")
print(run_cmd)
cmd = subprocess.run( run_cmd, stderr=subprocess.DEVNULL, stdout=subprocess.DEVNULL )
retcode = cmd.returncode

if (retcode != args.expected_return_code):
    print("Test " + args.executable + "\nreturned\t" + str(retcode) + "\nexpected return code was: " + str(args.expected_return_code))
    sys.exit(1)
print("Test " + args.executable + " passed")
