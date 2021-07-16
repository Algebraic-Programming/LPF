
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

set(LPF_HWLOC "" CACHE PATH "Path to HWloc installation directory")

if (LPF_HWLOC)
    find_file(HWLOC_HEADER
        NAMES hwloc.h
        PATHS ${LPF_HWLOC}/include
        DOC "hwloc include directory"
        NO_DEFAULT_PATH)

    find_library(HWLOC_LIBRARIES
        NAMES hwloc
        PATHS ${LPF_HWLOC}/lib
        DOC "hwloc library which is required for pinning"
        NO_DEFAULT_PATH)

    find_program(HWLOC_DISTRIB
        NAMES hwloc-distrib
        PATHS ${LPF_HWLOC}/bin
        DOC "hwloc command line tool hwloc-distrib"
        NO_DEFAULT_PATH)

    find_program(HWLOC_CALC
        NAMES hwloc-calc
        PATHS ${LPF_HWLOC}/bin
        DOC "hwloc command line itool hwloc-calc"
        NO_DEFAULT_PATH)
endif()

find_file(HWLOC_HEADER
        NAMES hwloc.h
        DOC "hwloc include directory")
get_filename_component(HWLOC_INCLUDE_DIR "${HWLOC_HEADER}" PATH)

find_library(HWLOC_LIBRARIES
        NAMES hwloc
        DOC "hwloc library which is required for pinning" )

find_program(HWLOC_DISTRIB
        NAMES hwloc-distrib
        DOC "hwloc command line tool hwloc-distrib" )


find_program(HWLOC_CALC
        NAMES hwloc-calc
        DOC "hwloc command line itool hwloc-calc" )

set(hwloc_found NO)
if (HWLOC_DISTRIB AND HWLOC_LIBRARIES AND HWLOC_HEADER AND HWLOC_CALC)
    set(hwloc_found YES)
    message( STATUS "Found hwloc library" )
endif()

option(HWLOC_FOUND "Whether HWloc library is found and can be used" ${hwloc_found})
if (HWLOC_FOUND)
    message(STATUS "Hwloc library will be used")
else()
    message(STATUS "hwloc library will *not* be used")
endif()
