#!/bin/bash

rm -rf /storage/users/gitlab-runner/lpf_repo
git clone --branch ci https://oath2:glpat-yqiQ3S1Emax8EoN91ycU@gitlab.huaweirc.ch/zrc-von-neumann-lab/spatial-computing/lpf/ /storage/users/gitlab-runner/lpf_repo
pushd /storage/users/gitlab-runner/lpf_repo
mkdir build
pushd build
../bootstrap.sh --functests; make -j32
make -j32