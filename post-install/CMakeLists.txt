
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

# Run post-install test
set( srcdir ${CMAKE_CURRENT_SOURCE_DIR})
set( builddir ${CMAKE_CURRENT_BINARY_DIR})
set( bindir "${INSTALL_BIN}" )
set( testdir "${CMAKE_SOURCE_DIR}/tests/functional" )
set( common "${CMAKE_SOURCE_DIR}/src/common" )
set( exampledir "${CMAKE_SOURCE_DIR}/examples/" )

# Debug tests do not work in some distributions (e.g., Ubuntu 22.04),
# hence they can be disabled during configuration with this option
option(RUN_DEBUG_TESTS ON)
configure_file( post-install-test.cmake.in post-install-test.cmake @ONLY )
install(SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/post-install-test.cmake)

