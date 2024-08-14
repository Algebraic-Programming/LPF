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

echo '.____   _____________________'
echo '|    |  \______   \_   _____/'
echo '|    |   |     ___/|    __)  '
echo '|    |___|    |    |     \   '
echo '|_______ \____|    \___  /   '
echo '        \/             \/    '
echo
echo 'Lightweight Parallel Foundations'
echo
echo 'Copyright (c) 2016-2021 by Huawei Technologies'
echo 'All rights reserved.'
echo
echo
echo "BUILD BOOTSTRAP SCRIPT"
echo "======================"
echo 

function FAIL()
{
   echo 
   echo "FATAL ERROR"
   echo "======================"
   echo 
   echo $1
   echo 
   echo "Bootstrap.sh aborted."
   exit 1
}

function abs_path()
{
    dir="$1"
    if [ ! -d "$dir" ]; then
        readlink -f -m "$dir" || echo $dir
    else
        pushd $dir > /dev/null 
        pwd
        popd > /dev/null
    fi
}

function print_configuration()
{
    echo "Configuration Options ${config_name:+for configuration named '${config_name}'}"
    echo "------------------------------------------------------"
    echo
    echo " - Build directory        = $builddir"
    echo
    echo " - Installation directory = $installdir"
    echo
    echo " - Build configuration    = $config"
    echo
    echo " - Build documentation    = $doc"
    echo
    echo " - Functional Tests       = $functests"
    echo
    echo " - Performance Tests      = $perftests"
    echo
}

srcdir=`dirname $0`
srcdir=`abs_path $srcdir`

# Take the current directory as build directory
builddir=`pwd`

# Parse command line parameters
installdir="$builddir"
config=Debug #Release
doc=OFF
functests=OFF
perftests=OFF
reconfig=no
CMAKE_EXE=cmake
unset mpicxx
unset mpicc
unset mpi_cmake_flags
unset mpiexec
unset extra_flags
unset perf_flags
unset hwloc
unset hwloc_found_flag
for arg
do
    case $arg in
       --prefix=*)
            installdir=`abs_path ${arg#--prefix=}`

            shift
            ;;

       --debug)
            config=Debug
            shift
            ;;

       --debug2)
            config=Debug
            extra_flags='-DCMAKE_CXX_FLAGS_DEBUG=-g -O0 -D_GLIBCXX_DEBUG=1'
            shift
            ;;

       --release)
            config=Release
            shift
            ;;

       --functests)
            functests=ON
            shift
            ;;

       --perftests)
            perftests=ON
            shift
            ;;

       --perftests=*)
            perftests=ON
            list=${arg#--perftests=}
            perf_flags="-DLPFLIB_PERFTESTS_PROCLIST=${list//,/;}"
            shift
            ;;

       --with-mpicxx=*)
            mpicxx="${arg#--with-mpicxx=}"
            mpi_cmake_flags="${mpi_cmake_flags} -DMPI_CXX_COMPILER=$mpicxx"
            shift
            ;;

        --with-mpicc=*)
            mpicc="${arg#--with-mpicc=}"
            mpi_cmake_flags="${mpi_cmake_flags} -DMPI_C_COMPILER=$mpicc"
            shift
            ;;

       --with-mpiexec=*)
            mpiexec="${arg#--with-mpiexec=}"
            mpi_cmake_flags="${mpi_cmake_flags} -DMPIEXEC=$mpiexec"
            shift;
            ;;

       --without-hwloc)
            hwloc_found_flag="-DHWLOC_FOUND=NO"
            hwloc=NO
            shift
            ;;

       --with-hwloc=*)
            hwloc="${arg#--with-hwloc=}"
            unset hwloc_found_flag
            shift
            ;;

       --with-cmake=*)
            CMAKE_EXE="${arg#--with-cmake=}"
	    shift;
	    ;;

       --config-name=*)
            config_name="${arg#--config-name=}"
            shift
            ;;

       --enable-doc)
            doc=ON
            shift
            ;;

       --disable-doc)
            doc=OFF
            shift
            ;;

       --reconfig)
            reconfig=yes
            shift
            ;;

       --help)
            less $srcdir/README
            exit 0
       ;;

       --*)
            echo "Unrecognized option '$arg'"
            exit 1
           ;;

       *) break
          ;;
    esac
done

# Options sanity check
if [ x$functests != xON ]; then
    if [ x$perftests == xON ]; then
        echo "Option error: setting --perftests also requires setting --functests."
        exit 1
    fi
fi
if ! command -v ${CMAKE_EXE} &> /dev/null
then
	echo "Cannot find cmake (tried \"${CMAKE_EXE}\")."
	exit 1
fi

echo
echo

# Configure LPF
if [ $reconfig = no -a -f CMakeCache.txt ]; then
  echo "Regenerating LPF build (consider using --reconfig)"
elif [ $reconfig = yes -a -f CMakeCache.txt ]; then
  rm -r CMakeCache.txt CMakeFiles
  echo "Reconfiguring LPF build"
else
  echo "Configuring LPF build"
fi

echo "--------------------------------------------------"
echo
${CMAKE_EXE} -Wno-dev \
      -DCMAKE_INSTALL_PREFIX="$installdir" \
      -DCMAKE_BUILD_TYPE=$config           \
      -DLPFLIB_MAKE_DOC=$doc         \
      -DLPFLIB_MAKE_TEST_DOC=$doc    \
      -DLPF_ENABLE_TESTS=$functests \
      -DLPFLIB_PERFTESTS=$perftests  \
      -DLPFLIB_CONFIG_NAME=${config_name:-${config}}\
      -DLPF_HWLOC="${hwloc}" \
      $hwloc_found_flag \
      $mpi_cmake_flags \
      "$extra_flags" \
      "$perf_flags" \
      "$@" $srcdir \
     || { echo FAIL "Failed to configure LPF; Please check your chosen configuration"; exit 1; }

echo
echo 
echo "DONE"
echo

print_configuration

echo "--- Note:"
echo "To build this project run 'make', to install 'make install'.  If you are"
echo "the lucky owner of a multi-core processor, consider using the '-j' option"
echo "of make."


