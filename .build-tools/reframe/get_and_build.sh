#!/bin/bash

rm -rf /storage/users/${USER}/lpf_repo
echo "CI_JOB_TOKEN: ${CI_JOB_TOKEN}"
TOKEN=$(cat $HOME/.PERSONAL_TOKEN)
echo "GITLAB_RUNNER_PERSONAL_TOKEN: ${TOKEN}"
git clone --branch ${CI_COMMIT_REF_NAME} https://oath2:${TOKEN}@gitlab.huaweirc.ch/zrc-von-neumann-lab/spatial-computing/lpf/ /storage/users/${USER}/lpf_repo
pushd /storage/users/${USER}/lpf_repo
mkdir build
pushd build
../bootstrap.sh --functests=i-agree-with-googletest-license; make -j32
make -j32

