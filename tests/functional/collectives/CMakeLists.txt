
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


set(c99_tests_sources
func_lpf_allcombine.cpp
func_lpf_allgather.cpp
func_lpf_allgather_overlapped.cpp
func_lpf_allreduce.cpp
func_lpf_alltoall.cpp
func_lpf_broadcast.cpp
func_lpf_broadcast_prime_size_object.cpp
func_lpf_broadcast_small_prime_size_object.cpp
func_lpf_collectives_init.cpp
func_lpf_collectives_init_overflow.cpp
func_lpf_combine.cpp
func_lpf_gather.cpp
func_lpf_reduce.cpp
func_lpf_scatter.cpp
func_lpf_zero_cost.cpp
)

foreach(LPF_IMPL_ID "ibverbs")
    set(debug ON)
    set(LPF_IMPL_CONFIG ${LPFLIB_CONFIG_NAME})

    set(mode)
    if (debug)
        set(mode "_debug")
    endif(debug)

    # add all source files except the ones we don't want
    foreach(testSource ${c99_tests_sources})
        string(REGEX REPLACE "(.${LPF_IMPL_ID})?.cpp$" "" baseName ${testSource})
        get_filename_component(baseName ${testSource} NAME_WE  )
        set(exeName "${baseName}_${LPF_IMPL_ID}_${LPF_IMPL_CONFIG}${mode}")

        message("Add test: ${exeName}")
        # Signature: function(add_gtest_mpi testName engines debug testSource)
        add_gtest_mpi(${exeName} ${ENGINES} ${LPF_IMPL_ID} ${debug}  FALSE "${CMAKE_CURRENT_SOURCE_DIR}/${testSource}")
    endforeach()

endforeach(LPF_IMPL_ID)
foreach(LPF_IMPL_ID "ibverbs")
    set(debug ON)
    set(LPF_IMPL_CONFIG ${LPFLIB_CONFIG_NAME})
    set(mode)
    if (debug)
        set(mode "_debug")
    endif()

    foreach(testSource ${c99_tests_sources})
        string(REGEX REPLACE "(.${LPF_IMPL_ID})?.cpp$" "" baseName ${testSource})
        get_filename_component(baseName ${testSource} NAME_WE  )
        set(exeName "${baseName}_${LPF_IMPL_ID}_${LPF_IMPL_CONFIG}${mode}")

        message("Try to gtest-discover ${exeName}")
        gtest_discover_tests(${exeName})
    endforeach(testSource)
endforeach(LPF_IMPL_ID)
