
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

# detect the maximum number of available processes
set( MAX_P)
execute_process( COMMAND getconf _NPROCESSORS_ONLN
                 OUTPUT_VARIABLE MAX_P
                OUTPUT_STRIP_TRAILING_WHITESPACE )
if (NOT MAX_P )
    set( MAX_P 2)
endif()

set( LPFLIB_PERFTESTS_PROCLIST ${MAX_P} CACHE STRING
        "An list of process numbers to run the performance tests on")

message( STATUS "Performance tests are included // list ${LPFLIB_PERFTESTS_PROCLIST}")

#Each test shouldn't take longer than this many seconds
set( LPFLIB_PERFTESTS_SOFT_DEADLINE "600" CACHE STRING
        "Soft deadline in seconds for each performance test")
math(EXPR deadline "${LPFLIB_PERFTESTS_SOFT_DEADLINE} + 300" )
set( LPFLIB_PERFTESTS_HARD_DEADLINE "${deadline}" CACHE STRING
        "Hard deadline in seconds for each performance test")

set( pinning_modes_pthread none compact scatter)
#MPI's usually pin their processes already
set( pinning_modes_mpirma none)
set( pinning_modes_mpimsg none)
#unpinned hybrid mode is too erratic
set( pinning_modes_hybrid compact scatter)

foreach(LPF_IMPL_ID ${ENGINES})
    set(LPF_IMPL_CONFIG ${LPFLIB_CONFIG_NAME})

    set( LPFLIB_PERFTESTS_${LPF_IMPL_ID}_PROCLIST ${LPFLIB_PERFTESTS_PROCLIST}
            CACHE STRING "A list of process numbers to run the ${LPF_IMPL_ID} performance tests on")

    set(exeName "alltoall-${LPF_IMPL_ID}-${LPF_IMPL_CONFIG}")
    set(corelib "lpf_core_${LPF_IMPL_ID}_${LPF_IMPL_CONFIG}")

    add_executable(${exeName} alltoall.c ../../src/common/rng.c )
    target_link_exe_with_core(${exeName} ${LPF_IMPL_ID})

    foreach( nprocs ${LPFLIB_PERFTESTS_${LPF_IMPL_ID}_PROCLIST})
    foreach( pinning ${pinning_modes_${LPF_IMPL_ID}} )
        add_test( NAME "perftest_${exeName}_${nprocs}_pin${pinning}"
           COMMAND bash "${CMAKE_BINARY_DIR}/lpfrun_build"
                    "-engine" "${LPF_IMPL_ID}"
                    "-np" ${nprocs}
                    "-probe" "2.0"
                    "-pinning" ${pinning}
                   $<TARGET_FILE:${exeName}> "perfresults.jlt"
                   ${LPFLIB_PERFTESTS_SOFT_DEADLINE} csv "p=${nprocs}.pin=${pinning}"
        )

        set_tests_properties( "perftest_${exeName}_${nprocs}_pin${pinning}"
                PROPERTIES TIMEOUT ${LPFLIB_PERFTESTS_HARD_DEADLINE} )
    endforeach()
    endforeach()

endforeach(LPF_IMPL_ID)

