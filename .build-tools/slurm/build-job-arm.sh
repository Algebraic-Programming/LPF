#!/bin/bash
#SBATCH -p ARM
#SBATCH --tasks-per-node 56
#SBATCH --cpus-per-task=1
#SBATCH --mem 0
#SBATCH -t 03:00:00
#SBATCH -o job-arm-%j.stdout
#SBATCH -e job-arm-%j.stderr
source $HOME/spack/share/spack/setup-env.sh
spack env activate hicr-arm
HASH=$(git rev-parse --verify HEAD)
echo $HASH
mkdir build-arm ; cd build-arm
../bootstrap.sh --functests 
make -j56
ctest
