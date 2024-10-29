
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

set(LPF_IMPL_CONFIG ${LPFLIB_CONFIG_NAME})

set(libname      "lpf_debug")
set(comlib       "lpf_common_${LPFLIB_CONFIG_NAME}")

add_library( ${libname}
    core.cpp
    rwconflict.cpp
    $<TARGET_OBJECTS:${comlib}>
)
target_link_libraries( ${libname} ${LIB_POSIX_THREADS})
target_include_directories( ${libname} PRIVATE ${MPI_C_INCLUDE_PATH})
set_target_properties(${libname} PROPERTIES 
        SOVERSION ${SOVERSION}
)

install(TARGETS ${libname} EXPORT lpf
        RUNTIME DESTINATION ${INSTALL_BIN}
        LIBRARY DESTINATION ${INSTALL_LIB}
        ARCHIVE DESTINATION ${INSTALL_LIB}
       )

add_gtest(rwconflict_test "pthread" ON rwconflict.t.cpp rwconflict.cpp)
