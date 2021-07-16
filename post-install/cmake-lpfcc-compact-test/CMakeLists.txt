
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

cmake_minimum_required(VERSION 2.8)
project(LPF_CMAKE_TEST)

set(CMAKE_C_FLAGS "-lbsplib -engine pthread" )
set(CMAKE_CXX_FLAGS "-lbsplib -engine pthread" )

set(LPF_LPFRUN $ENV{LPFRUN})

add_executable( hello_c hello.c)
add_executable( hello_cpp hello.cpp)

enable_testing()
add_test( NAME hello_c_test 
	COMMAND ${LPF_LPFRUN} -n 2 -engine pthread $<TARGET_FILE:hello_c> )

add_test( NAME hello_cpp_test 
	COMMAND ${LPF_LPFRUN} -n 2 -engine pthread $<TARGET_FILE:hello_cpp> )

