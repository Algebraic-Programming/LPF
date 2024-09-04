
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

# All test sources have file names as bla.c
#file(GLOB AllTestSources "*.cpp" )
# All test sources which are specific to some engine are of the form
# bla.pthread.c
#file(GLOB AllSpecificTestSources "*.*.cpp")
# All generic test sources don't have the two dots in their name
#file(GLOB AllGenericTestSources "*.c")
#file(GLOB AllGenericTestSources "*.cpp")
#list(REMOVE_ITEM AllGenericTestSources ${AllSpecificTestSources})

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
    #func_lpf_hook_simple.mpirma.cpp
    #func_lpf_hook_simple.pthread.cpp
    #func_lpf_hook_subset.mpimsg.cpp
    #func_lpf_hook_tcp.mpirma.cpp
    #func_lpf_hook_tcp_timeout.mpirma.cpp
    func_lpf_probe_parallel_full.cpp
    func_lpf_probe_parallel_nested.cpp
    func_lpf_probe_root.cpp
    func_lpf_put_and_get_overlapping.cpp
    func_lpf_put_parallel_alltoall.cpp
    #func_lpf_put_parallel_bad_pattern.cpp <= in exception_list
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
    # debug:
    debug/func_lpf_debug_deregister_non_existing_slot.cpp
    debug/func_lpf_debug_exec_null_f_symbols.cpp
    debug/func_lpf_debug_exec_null_input.cpp
    debug/func_lpf_debug_exec_null_output.cpp
    debug/func_lpf_debug_exec_null_spmd.cpp
    debug/func_lpf_debug_get_local_src_slot.cpp
    debug/func_lpf_debug_get_overflow_dst_offset.cpp
    debug/func_lpf_debug_get_overflow_src_offset.cpp
    debug/func_lpf_debug_get_read_past_source_memory_global_known_at_sync.cpp
    debug/func_lpf_debug_get_read_past_source_memory_global_known_before_sync.cpp
    debug/func_lpf_debug_get_too_many_requests.cpp
    debug/func_lpf_debug_get_too_many_requests_remote.cpp
    debug/func_lpf_debug_get_too_many_requests_self.cpp
    debug/func_lpf_debug_get_unknown_dest_slot.cpp
    debug/func_lpf_debug_get_unknown_source_pid.cpp
    debug/func_lpf_debug_get_unknown_source_slot.cpp
    debug/func_lpf_debug_get_write_past_dest_memory_global.cpp
    debug/func_lpf_debug_get_write_past_dest_memory_local.cpp
    debug/func_lpf_debug_global_deregister_mismatch.cpp
    debug/func_lpf_debug_global_deregister_order_mismatch.cpp
    debug/func_lpf_debug_global_deregister_unequal.cpp
    debug/func_lpf_debug_global_register_null_memreg.cpp
    debug/func_lpf_debug_hook_null_f_symbols.pthread.cpp
    debug/func_lpf_debug_hook_null_input.pthread.cpp
    debug/func_lpf_debug_hook_null_output.pthread.cpp
    debug/func_lpf_debug_hook_null_spmd.pthread.cpp
    debug/func_lpf_debug_local_register_null_memreg.cpp
    debug/func_lpf_debug_put_after_deregister_dest_after_sync.cpp
    debug/func_lpf_debug_put_after_deregister_dest.cpp
    debug/func_lpf_debug_put_after_deregister_source_after_sync.cpp
    debug/func_lpf_debug_put_after_deregister_source.cpp
    debug/func_lpf_debug_put_get_too_many_requests.cpp
    debug/func_lpf_debug_put_get_too_many_requests_remote.cpp
    debug/func_lpf_debug_put_local_dest_slot.cpp
    debug/func_lpf_debug_put_overflow_dst_offset.cpp
    debug/func_lpf_debug_put_overflow_src_offset.cpp
    debug/func_lpf_debug_put_read_past_source_memory_global.cpp
    debug/func_lpf_debug_put_read_past_source_memory_local.cpp
    debug/func_lpf_debug_put_read_write_conflict_among_many.cpp
    debug/func_lpf_debug_put_read_write_conflict.cpp
    debug/func_lpf_debug_put_too_many_requests.cpp
    debug/func_lpf_debug_put_too_many_requests_remote.cpp
    debug/func_lpf_debug_put_too_many_requests_self.cpp
    debug/func_lpf_debug_put_unknown_dest_pid.cpp
    debug/func_lpf_debug_put_unknown_dest_slot.cpp
    debug/func_lpf_debug_put_unknown_source_slot.cpp
    debug/func_lpf_debug_put_write_past_dest_memory_global_known_at_sync.cpp
    debug/func_lpf_debug_put_write_past_dest_memory_global_known_before_sync.cpp
    debug/func_lpf_debug_register_global_dst_unsynced.cpp
    debug/func_lpf_debug_register_global_src_unsynced.cpp
    )


foreach(LPF_IMPL_ID "ibverbs")
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

        message("Add test: ${exeName}")
        # Signature: function(add_gtest_mpi testName engines debug testSource)
        add_gtest_mpi(${exeName} ${LPF_IMPL_ID} ${debug} ${CMAKE_CURRENT_SOURCE_DIR}/${testSource})
    endforeach()

endforeach(LPF_IMPL_ID)
foreach(LPF_IMPL_ID "ibverbs")
    set(debug ON)
    set(LPF_IMPL_CONFIG ${LPFLIB_CONFIG_NAME})
    set(mode)
    if (debug)
        set(mode "_debug")
    endif()

    foreach(testSource ${test_sources})
        string(REGEX REPLACE "(.${LPF_IMPL_ID})?.cpp$" "" baseName ${testSource})
        get_filename_component(baseName ${testSource} NAME_WE  )
        set(exeName "${baseName}_${LPF_IMPL_ID}_${LPF_IMPL_CONFIG}${mode}")

        message("Try to gtest-discover ${exeName}")
        gtest_discover_tests(${exeName})
    endforeach(testSource)
endforeach(LPF_IMPL_ID)


include_directories(.)

add_subdirectory(c99)


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

