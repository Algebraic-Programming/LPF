import reframe as rfm
import reframe.utility.sanity as sn
import os

@rfm.simple_test
class LPFFuncTests(rfm.RunOnlyRegressionTest):
    def __init__(self):
        self.maintainers = ['Kiril Dichev']
        self.num_tasks = 64
        self.sourcesdir = '.'
        self.prerun_cmds = ['source get_and_build.sh']
        self.valid_systems = ['BZ:arm-sequential']
        self.valid_prog_environs = ['*']
        self.executable = 'ctest'
        self.executable_opts = ['--test-dir', '/storage/distscratch/okorak/lpf_repo/build']
        self.sanity_patterns = sn.assert_found('Tests', self.stdout)

