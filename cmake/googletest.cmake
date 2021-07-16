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

include(ExternalProject)
set(gtest_prefix "${CMAKE_CURRENT_BINARY_DIR}/gtest")
set(GTEST_DOWNLOAD_URL
         "https://github.com/google/googletest/archive/release-1.8.1.tar.gz"
         CACHE STRING "File location or URL from where to download Google Test")
set(GTEST_LICENSE_URL
         "https://github.com/google/googletest/blob/release-1.8.1/LICENSE"
         CACHE STRING "File location or URL to license file of Google Test")
option(GTEST_AGREE_TO_LICENSE
       "User agreement with license of Google Testing Framework, available at ${GOOGLE_LICENSE_URL}"
       OFF)

if (NOT GTEST_AGREE_TO_LICENSE)
    message(SEND_ERROR "The LPF test suite requires agreement with the license of Google Test. Either disable LPF_ENABLE_TESTS or agree with the license by enabling GTEST_AGREE_TO_LICENSE")
endif()

ExternalProject_Add(
        GoogleTest
        PREFIX ${gtest_prefix}
        INSTALL_DIR ${gtest_prefix}
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${gtest_prefix}
                   -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
                   -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
                   -Dgtest_disable_pthreads=ON
        URL ${GTEST_DOWNLOAD_URL}
        EXCLUDE_FROM_ALL YES)
ExternalProject_Add_StepTargets(GoogleTest install)
add_library(gtest_main STATIC IMPORTED)
add_dependencies(gtest_main GoogleTest-install)
add_library(gtest STATIC IMPORTED)
add_dependencies(gtest GoogleTest-install)

# In order to prevent failure of the next set_target_properties
# INTERFACE_INCLUDE_DIRECTORIES, make this directory before it is made by the
# GoogleTest external project
file(MAKE_DIRECTORY ${gtest_prefix}/include)
set_target_properties(gtest_main
        PROPERTIES IMPORTED_LOCATION ${gtest_prefix}/${CMAKE_INSTALL_LIBDIR}/libgtest_main.a
                   INTERFACE_INCLUDE_DIRECTORIES ${gtest_prefix}/include
                   INTERFACE_LINK_LIBRARIES ${gtest_prefix}/${CMAKE_INSTALL_LIBDIR}/libgtest.a)
set_target_properties(gtest
        PROPERTIES IMPORTED_LOCATION ${gtest_prefix}/${CMAKE_INSTALL_LIBDIR}/libgtest.a
                   INTERFACE_INCLUDE_DIRECTORIES ${gtest_prefix}/include)
