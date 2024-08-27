#!/bin/bash

#
#   Copyright 2021 Huawei Technologies Co., Ltd.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

shopt -s extglob

builddir=`pwd`
dir=`dirname $0`
defaultMaxProcs=5
defaultNodes=2   # This makes the hybrid implementation sensitive to errors
intermOutput=output
lpf_impl_id=$1
lpf_impl_config=$2
log=tests-${lpf_impl_id}-${lpf_impl_config}.log
junitfile=$3
loglevel=1
shift
shift
shift

function get()
{
  sed -ne 's/.*\\'"$1"'[[:space:]]*\(.*\)$/\1/p'
}

function log
{
    echo "$@" | tee -a $log
}

if [ `uname` = Darwin ]; then
    # Non GNU date can't print nanoseconds
    function getTime()
    {
        date +%s
    }

    function nproc() {
        /usr/sbin/sysctl -o machdep.cpu.core_count | sed -e 's/.*://'
    }
else
    function getTime()
    {
        date +%s.%N
    }

    if which nproc; then
      nproc_exe=`which nproc`
      function nproc() {
          $nproc_exe
      }
    else
      function nproc() {
          echo $defaultMaxProcs
      }
    fi
fi

# Adjust default max number of processes
if [ `nproc` -lt $defaultMaxProcs ]; then
  defaultMaxProcs=`nproc`
fi

# Some systems don't have DC calculator
echo | dc
if [ $? -ne 0 ]; then
    # define a dummy implementation, because it is only used to report
    # the time needed by every test
    function dc() {
        echo 0
    }
fi


function lpfrun
{
    bash ../../lpfrun_build "$@"
}

XUNIT_TESTS=0
XUNIT_FAILURES=0
XUNIT_ERRORS=0
XUNIT_TOTALTIME=0
rm -f ${junitfile} ${junitfile}.txt

if [ `uname` = Darwin ]; then
  function junit
  {
    return
  }
else

  function junit
  {
    case $1 in
        add)     name=$2
                 success=$3
                 t=$4
                 shift
                 shift
                 shift
                 shift


                 XUNIT_TESTS=$((XUNIT_TESTS + 1))
                 XUNIT_TOTALTIME=$( (echo $XUNIT_TOTALTIME; echo $t; echo '+'; echo 'p') | dc )
                 echo "<testcase name=\"${name}\" status=\"run\" time=\"${t}\" classname=\"${lpf_impl_id}_${lpf_impl_config}\">" >> ${junitfile}.txt
                 if [ $success -eq 0 ]; then
                     XUNIT_FAILURES=$((XUNIT_FAILURES + 1))
                     echo "<failure><![CDATA[$@" >> ${junitfile}.txt
                     cat >> ${junitfile}.txt
                     echo "]]></failure>" >> ${junitfile}.txt
                 fi
                 echo "</testcase>" >> ${junitfile}.txt
                 ;;


        write)   echo "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" > $junitfile
                 echo "<testsuites tests=\"${XUNIT_TESTS}\" failures=\"${XUNIT_FAILURES}\" disabled=\"0\" errors=\"${XUNIT_ERRORS}\" timestamp=\"`date -Iseconds`\" time=\"${XUNIT_TOTALTIME}\" name=\"API\" >" >> $junitfile
                 echo "<testsuite name=\"${lpf_impl_id}_${lpf_impl_config}\" tests=\"${XUNIT_TESTS}\" failures=\"${XUNIT_FAILURES}\" disabled=\"0\" error=\"${XUNIT_ERRORS}\" time=\"${XUNIT_TOTALTIME}\">" >> $junitfile
                 cat ${junitfile}.txt >> $junitfile
                 echo "</testsuite></testsuites>" >> $junitfile
                 ;;
    esac

  }
fi

rm -f $log
log "============================================================================"
log "RUNNING LPF API TESTS"
allSuccess=1
suffix="_${lpf_impl_id}_${lpf_impl_config}"
for testexe in $(find . -name "*${suffix}" -or -name "*${suffix}_debug")
do
    if [[ $testexe == *"bsplib_hpget_many"* ]] ; then
        testname=${testexe%_debug}
        if [ "x${testname}" != "x${testexe}" ] ; then 
            mode=debug;
        else
            mode=default;
        fi

        testname=$(basename ${testname%${suffix}})
        log "testname:", $testname
        testCase=$(find $dir -name "${testname}.c" -or -name "${testname}.${lpf_impl_id}.c")
        description=`get 'test' < $testCase`
        message=`get 'return Message:' < $testCase`
        exitCode=`get 'return Exit code:' < $testCase`
        minProcs=`get 'pre[[:space:]]*P >=' < $testCase`
        maxProcs=`get 'pre[[:space:]]*P <=' < $testCase`
        extraParams=`get 'note Extra lpfrun parameters:' < $testCase`
        indepProcs=`get 'note Independent processes:' < $testCase`

        if echo "${testexe}" | grep -qf $dir/exception_list ; then
            log "----------------------------------------------------------------------------"
            log "  IGNORING:              $testname"
            log "    Description:         $description"
            continue
        fi

        if [ x$testname = x ]; then
            log "Warning: Can't read testname from $testCase. Test case skipped" ;
            allSuccess=0;
            continue
        fi
        if [ x$exitCode = x ]; then
            log "Error: Can't read expected exit code from $testCase. Test case skipped" ;
            allSuccess=0;
            continue
        fi
        if [ '!' '(' $exitCode -ge 0 ')' ]; then
            log "Error: Can't read expected exit code from $testCase. Test case skipped" ;
            allSuccess=0;
            continue
        fi
        if [ x$minProcs = x ]; then
            log "Error: Can't determine lower bound of processes for $testCase. Test case skipped" ;
            allSuccess=0;
            continue
        fi
        if [ '!' '(' $minProcs -ge 1 ')' ]; then
            log "Error: Lower bound of processes is illegal for test case $testCase. Test case skipped" ;
            allSuccess=0;
            continue
        fi
        if [ x$maxProcs = x ]; then
            maxProcs=$defaultMaxProcs
        fi
        if [ '!' '(' $maxProcs -ge 1 ')' ]; then
            log "Error: Upper bound of processes is illegal for test case $testCase. Test case skipped"
            allSuccess=0;
            continue
        fi

        if [ x$indepProcs '!=' xyes ]; then
            indepProcs=no
        fi

        log "----------------------------------------------------------------------------"
        log "  RUNNING:               $testname   ( $mode )"
        log "    Description:         $description"
        log "    Number of processes: $minProcs - $maxProcs"
        log "    Engine:              $lpf_impl_id"
        log "    Configuration:       $lpf_impl_config"
        log "    Extra lpfrun params: $extraParams"
        log "    Independent processes: $indepProcs"
        log

#$lpfcc $testCase -o ${testname}.exe -Wall -Wextra >> $log 2>&1
#  compilation=$?

#  if [ '!' $compilation -eq 0 ]; then
#    log "  TEST FAILURE: Failed to compile $testCase"
#    allSuccess=0;
#    continue
#  fi

setSuccess=1
for (( processes=$minProcs; processes <= $maxProcs; ++processes ))
do
    success=1
    t0=`getTime`
    if [ $indepProcs = no ]; then
        # The normal way of running a test

        lpfrun -engine $lpf_impl_id -log $loglevel \
            -n $processes -N $defaultNodes ${extraParams} \
            "$@" ./${testexe}  > $intermOutput 2>&1 
                    actualExitCode=$?
                else
                    # this way of running processes is required to test implementation of 
                    # lpf_hook on MPI implementations

                    rm $intermOutput
                    touch $intermOutput
                    for (( p = 0; p < processes; ++p ))
                    do
                        lpfrun -engine $lpf_impl_id -log $loglevel -np 1 ${extraParams} "$@" \
                            ./${testexe} $p ${processes}  >> $intermOutput 2>&1 &
                        done
                        wait `jobs -p`
                        actualExitCode=$?
    fi
    t1=`getTime`
    t=$( ( echo $t1 ; echo $t0; echo "-"; echo "p" ) | dc )

    cat $intermOutput >> $log
    # NOTE: Only two exit codes are recognized: failure and success. That's because most
    # MPI implementations mangle the exit code. 
    msg=
    if [ \( $actualExitCode -eq 0 -a $exitCode -ne 0 \) -o \
        \( $actualExitCode -ne 0 -a $exitCode -eq 0 \) ]; then
            msg="  TEST FAILURE: Expected exit code $exitCode does not match actual exit code $actualExitCode for $testCase on $processes processes"
            log "$msg"
            allSuccess=0;
            setSuccess=0
            success=0
    fi
    if [ "x$message" != x ]; then
        if grep -q "$message" $intermOutput ; then
            let noop=0;
        else
            msg="  TEST FAILURE: Expected messages does not match for $testCase on $processes processes"
            log "$msg"
            allSuccess=0
            setSuccess=0
            success=0
        fi
    fi
    junit add "$testname.$processes" $success $t "$msg" < $intermOutput
done
if [ $setSuccess -eq 1 ]; then
    log "TEST SUCCESS"
fi
fi
done

junit write

log "----------------------------------------------------------------------------"
if [ $allSuccess -eq 0 ]; then
  log "ONE OR MORE TEST FAILURES"
  exit 1
else
  log "ALL TESTS SUCCESSFUL"
  exit 0
fi
