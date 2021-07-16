
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

include_directories(${CMAKE_SOURCE_DIR}/include)

add_executable(list list_test.cpp lpf_random.c)
if (CMAKE_BUILD_TYPE STREQUAL Debug)
    target_include_directories(list BEFORE PRIVATE ${CMAKE_SOURCE_DIR}/include/debug)
endif()

if (CMAKE_BUILD_TYPE STREQUAL Debug)
    target_link_libraries(list "lpf_hl_debug" "lpf_debug")
else()
    target_link_libraries(list "lpf_hl")
endif()
target_link_exe_with_core(list)

