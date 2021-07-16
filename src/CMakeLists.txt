
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

add_subdirectory(imp)
add_subdirectory(common)
add_subdirectory(pthreads)
add_subdirectory(MPI)
add_subdirectory(hybrid)
add_subdirectory(core-libraries)
add_subdirectory(bsplib)
add_subdirectory(debug)

set(lpf_cflags "${lpf_cflags}" PARENT_SCOPE)
set(lpf_lib_link_flags "${lpf_lib_link_flags}" PARENT_SCOPE)
set(lpf_exe_link_flags "${lpf_exe_link_flags}" PARENT_SCOPE)

