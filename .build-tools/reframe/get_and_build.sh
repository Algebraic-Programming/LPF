#!/bin/bash

rm -rf /storage/users/gitlab-runner/lpf_repo
git clone --branch ${CI_COMMIT_REF_NAME} https://oath2:glpat-xvYANSkTDdET28F9jBxb@gitlab.huaweirc.ch/zrc-von-neumann-lab/spatial-computing/lpf/ /storage/users/gitlab-runner/lpf_repo
pushd /storage/users/gitlab-runner/lpf_repo
mkdir build
pushd build
../bootstrap.sh --functests; make -j32
make -j32
