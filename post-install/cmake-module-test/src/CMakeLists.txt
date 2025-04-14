
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

cmake_minimum_required(VERSION 3.10)
project(findlpf_test)

find_package(lpf REQUIRED CONFIG)

set(EXE_NAME test_FindLPF)

set(TEST_ENGINE mpimsg CACHE STRING
        "engine with which this test should be run")

add_executable(${EXE_NAME} main.c)
target_link_lpf(${EXE_NAME} ENGINE ${TEST_ENGINE} BSPLIB PRIVATE})

add_executable(univ_${EXE_NAME} main.c)
target_link_lpf(univ_${EXE_NAME} BSPLIB PRIVATE})

enable_testing()
add_test( NAME test_cmake_${TEST_ENGINE}
          COMMAND ${LPF_LPFRUN} -n 2 -engine ${TEST_ENGINE} $<TARGET_FILE:${EXE_NAME}> )

add_test( NAME test_cmake_univ_${TEST_ENGINE}
          COMMAND ${LPF_LPFRUN} -n 2 -engine ${TEST_ENGINE} $<TARGET_FILE:univ_${EXE_NAME}> )


