import argparse
import subprocess
import sys

parser = argparse.ArgumentParser( description='Death test launcher' )
parser.add_argument("-e", "--engine", type=str)
parser.add_argument("-L", "--parallel_launcher", type=str)
parser.add_argument("-p", "--min_process_count", type=int)
parser.add_argument("-P", "--max_process_count", type=int)
parser.add_argument("-t", "--lpf_probe_timer", type=float)
parser.add_argument("-R", "--expected_return_code", type=int)
parser.add_argument( 'cmd', nargs=argparse.REMAINDER )
args = parser.parse_args()
if args.cmd[1] == "--gtest_list_tests":
    run_cmd = [args.cmd[0], args.cmd[1]]
    cmd = subprocess.run( run_cmd, capture_output=True)
else:
    for i in range(args.min_process_count, args.max_process_count+1):
        if args.lpf_probe_timer > 0.0:
            run_cmd = [args.parallel_launcher, '-engine', args.engine, '-probe', str(args.lpf_probe_timer), '-n', str(i)] + args.cmd
        else:
            run_cmd = [args.parallel_launcher, '-engine', args.engine, '-n', str(i)] + args.cmd
        print("Run command: ")
        print(run_cmd)
        cmd = subprocess.run( run_cmd, capture_output=True)
        print("Test returned code = " + str(cmd.returncode))
        retcode = cmd.returncode
        if (retcode != args.expected_return_code):
            print("Test " + args.cmd[0] + args.cmd[1] + "\nreturned\t" + str(retcode) + "\nexpected return code was: " + str(args.expected_return_code))
            sys.exit(1)
    print("Test " + args.cmd[0] + args.cmd[1] + " passed")
