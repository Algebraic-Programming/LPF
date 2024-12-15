
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

set(test_sources
    func_bsplib_example_lpf_sum.cpp
    func_bsplib_example_lpf_sum_unsafemode.cpp
    func_bsplib_example_put_array.cpp
    func_bsplib_example_put_array_unsafemode.cpp
    func_bsplib_example_reverse.cpp
    func_bsplib_example_reverse_unsafemode.cpp
    func_bsplib_get_exceptions.cpp
    func_bsplib_get_normal.cpp
    func_bsplib_get_normal_unsafemode.cpp
    func_bsplib_get_twice_on_same_remote.cpp
    func_bsplib_get_twice_on_same_remote_unsafemode.cpp
    func_bsplib_getput_same_dest.cpp
    func_bsplib_getput_same_dest_unsafemode.cpp
    func_bsplib_getput_same_remote.cpp
    func_bsplib_getput_same_remote_unsafemode.cpp
    func_bsplib_getput_zero_bytes.cpp
    func_bsplib_hpget_many.cpp
    func_bsplib_hpput_many.cpp
    func_bsplib_hpsend_many.cpp
    func_bsplib_nprocs.cpp
    func_bsplib_pid.cpp
    func_bsplib_pushpopreg_ambiguous.cpp
    func_bsplib_pushpopreg_different_variables.cpp
    func_bsplib_pushpopreg_exceptions.cpp
    func_bsplib_pushpopreg_many_same.cpp
    func_bsplib_pushpopreg_normal.cpp
    func_bsplib_pushpopreg_normal_unsafemode.cpp
    func_bsplib_pushpopreg_null.cpp
    func_bsplib_pushpopreg_pop_before_put.cpp
    func_bsplib_pushpopreg_pop_before_put_unsafemode.cpp
    func_bsplib_pushpopreg_pop_on_one_process.cpp
    func_bsplib_pushpopreg_push_on_one_process.cpp
    func_bsplib_pushpopreg_same_growing_memory.cpp
    func_bsplib_pushpopreg_same_shrinking_memory.cpp
    func_bsplib_pushpopreg_two_pops_before_two_puts.cpp
    func_bsplib_put_exceptions.cpp
    func_bsplib_put_normal.cpp
    func_bsplib_put_normal_unsafemode.cpp
    func_bsplib_send_empty_tag.cpp
    func_bsplib_send_non_empty_tag.cpp
    func_bsplib_send_none.cpp
    func_bsplib_send_null.cpp
    func_bsplib_send_one.cpp
    func_bsplib_send_one_unsafemode.cpp
    func_bsplib_set_different_tag_size.cpp
    func_bsplib_set_tag_size.cpp
    func_bsplib_sync_except_p0.cpp
    func_bsplib_sync_only_p0.cpp
    func_bsplib_time.cpp
    func_lpf_deregister_parallel_multiple.cpp
    func_lpf_deregister_parallel_single.cpp
    func_lpf_exec_multiple_call_single_arg_dual_proc.cpp
    func_lpf_exec_nested_call_single_arg_dual_proc.cpp
    func_lpf_exec_single_call_no_arg_max_proc.cpp
    func_lpf_exec_single_call_no_arg_single_proc.cpp
    func_lpf_exec_single_call_single_arg_dual_proc.cpp
    func_lpf_exec_single_call_single_arg_max_proc_early_exit_one.cpp
    func_lpf_exec_single_call_single_arg_max_proc_early_exit_zero.cpp
    func_lpf_exec_single_call_single_arg_single_proc.cpp
    func_lpf_get_parallel_alltoall.cpp
    func_lpf_get_parallel_huge.cpp
    func_lpf_get_parallel_overlapping_complete.cpp
    func_lpf_get_parallel_overlapping_pyramid.cpp
    func_lpf_get_parallel_overlapping_rooftiling.cpp
    func_lpf_get_parallel_single.cpp
    func_lpf_probe_parallel_full.cpp
    func_lpf_probe_parallel_nested.cpp
    func_lpf_probe_root.cpp
    func_lpf_put_and_get_overlapping.cpp
    func_lpf_put_parallel_alltoall.cpp
    func_lpf_put_parallel_big.cpp
    func_lpf_put_parallel_huge.cpp
    func_lpf_put_parallel_overlapping_complete.cpp
    func_lpf_put_parallel_overlapping_pyramid.cpp
    func_lpf_put_parallel_overlapping_rooftiling.cpp
    func_lpf_put_parallel_single.cpp
    func_lpf_register_and_deregister_irregularly.cpp
    func_lpf_register_and_deregister_many_global.cpp
    func_lpf_register_global_parallel_grow.cpp
    func_lpf_register_global_parallel_multiple.cpp
    func_lpf_register_global_parallel_shrink.cpp
    func_lpf_register_global_root_multiple.cpp
    func_lpf_register_global_root_single.cpp
    func_lpf_register_local_parallel_multiple.cpp
    func_lpf_resize_delayed_shrinking_memory_registers.cpp
    func_lpf_resize_delayed_shrinking_message_queues.cpp
    func_lpf_resize_parallel_five.cpp
    func_lpf_resize_root_five.cpp
    func_lpf_resize_root_outofmem.cpp
    func_lpf_resize_root_zero.cpp
    macro_LPF_VERSION.cpp
    type_lpf_spmd_t.cpp
    type_lpf_t.cpp
    )

foreach (LPF_IMPL_ID ${ENGINES})
    set(debug ON)
    set(LPF_IMPL_CONFIG ${LPFLIB_CONFIG_NAME})

    set(mode)
    if (debug)
        set(mode "_debug")
    endif(debug)

    # add all source files except the ones we don't want
    foreach(testSource ${test_sources})
        string(REGEX REPLACE "(.${LPF_IMPL_ID})?.cpp$" "" baseName ${testSource})
        get_filename_component(baseName ${testSource} NAME_WE  )
        set(exeName "${baseName}_${LPF_IMPL_ID}_${LPF_IMPL_CONFIG}${mode}")

        # Following tests are NOT run with zero engine:
        # - bsplib tests, as their semantics is not clear
        # - simple early_exit tests fail, as only a subset of
        #   processes call lpf_sync - zero engine does not support that,
        #   and lpf_sync has an implicit barrier in zero engine
        # - overlapping tests perform conflict resolution, but
        #   zero engine does not support that
        string(REGEX MATCH "overlapping|early|bsplib" foundTest ${testSource})
        if (NOT ${LPF_IMPL_ID} STREQUAL "zero") 
            add_gtest(${exeName} ${LPF_IMPL_ID} ${debug} "${CMAKE_CURRENT_SOURCE_DIR}/${testSource}")
        elseif ("${foundTest}" STREQUAL "")
            add_gtest(${exeName} ${LPF_IMPL_ID} ${debug} "${CMAKE_CURRENT_SOURCE_DIR}/${testSource}")
        endif()

    endforeach(testSource)

endforeach(LPF_IMPL_ID)

# Individual test for NOC (Non-coherence) protocol, only for zero engine
# (part of HiCR project)
set(LPF_IMPL_CONFIG ${LPFLIB_CONFIG_NAME})
set(mode "")
set(exeName "func_lpf_test_noc_ring_zero_${LPF_IMPL_CONFIG}${mode}")
add_gtest(${exeName} "zero" OFF "${CMAKE_CURRENT_SOURCE_DIR}/func_lpf_test_noc_ring.cpp")

# start of engine-specific tests
foreach (LPF_IMPL_ID ${ENGINES})
    if ("${LPF_IMPL_ID}" STREQUAL "pthread" OR "${LPF_IMPL_ID}" STREQUAL "mpirma")
        foreach(testSource func_lpf_hook_simple.${LPF_IMPL_ID}.cpp)
            string(REGEX REPLACE "(.${LPF_IMPL_ID})?.cpp$" "" baseName ${testSource})
            get_filename_component(baseName ${testSource} NAME_WE  )
            set(exeName "${baseName}_${LPF_IMPL_ID}_${LPF_IMPL_CONFIG}${mode}")
            add_gtest(${exeName} ${LPF_IMPL_ID} ON "${CMAKE_CURRENT_SOURCE_DIR}/${testSource}")
        endforeach(testSource)
    endif()
    if ("${LPF_IMPL_ID}" STREQUAL "mpimsg")
        add_gtest(func_lpf_hook_subset.mpimsg mpimsg ON "${CMAKE_CURRENT_SOURCE_DIR}/func_lpf_hook_subset.mpimsg.cpp")
    endif()
    if ("${LPF_IMPL_ID}" STREQUAL "mpirma")
        add_gtest(func_lpf_hook_tcp_timeout.mpirma mpirma ON "${CMAKE_CURRENT_SOURCE_DIR}/func_lpf_hook_tcp_timeout.mpirma.cpp")
    endif()
endforeach(LPF_IMPL_ID)
# end of engine-specific tests

include_directories(.)
add_subdirectory(debug)
add_subdirectory(collectives)


option(LPFLIB_MAKE_TEST_DOC "Build the test documentation" OFF)
if (LPFLIB_MAKE_TEST_DOC)
    find_package(Doxygen REQUIRED)

    # Generate Doxygen config file
    configure_file( unittests.cfg.in  unittests.cfg  @ONLY)

    # Run Doxygen
    add_custom_target( testdoc ALL
            DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/doc/html/index.html
            COMMENT "Generating API documentation"
            )
    add_custom_command( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/doc/html/index.html
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/unittests.cfg
            COMMENT "Running Doxygen"
            )
endif()

