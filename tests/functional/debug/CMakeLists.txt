
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
set(debug_test_sources
    func_lpf_debug_deregister_non_existing_slot.cpp
    func_lpf_debug_exec_null_f_symbols.cpp
    func_lpf_debug_exec_null_input.cpp
    func_lpf_debug_exec_null_output.cpp
    func_lpf_debug_exec_null_spmd.cpp
    func_lpf_debug_get_local_src_slot.cpp
    func_lpf_debug_get_overflow_dst_offset.cpp
    func_lpf_debug_get_overflow_src_offset.cpp
    func_lpf_debug_get_read_past_source_memory_global_known_at_sync.cpp
    func_lpf_debug_get_read_past_source_memory_global_known_before_sync.cpp
    func_lpf_debug_get_too_many_requests.cpp
    func_lpf_debug_get_too_many_requests_remote.cpp
    func_lpf_debug_get_too_many_requests_self.cpp
    func_lpf_debug_get_unknown_dest_slot.cpp
    func_lpf_debug_get_unknown_source_pid.cpp
    func_lpf_debug_get_unknown_source_slot.cpp
    func_lpf_debug_get_write_past_dest_memory_global.cpp
    func_lpf_debug_get_write_past_dest_memory_local.cpp
    func_lpf_debug_global_deregister_mismatch.cpp
    func_lpf_debug_global_deregister_order_mismatch.cpp
    func_lpf_debug_global_deregister_unequal.cpp
    func_lpf_debug_global_register_null_memreg.cpp
    #func_lpf_debug_hook_null_f_symbols.pthread.cpp
    #func_lpf_debug_hook_null_input.pthread.cpp
    #func_lpf_debug_hook_null_output.pthread.cpp
    #func_lpf_debug_hook_null_spmd.pthread.cpp
    func_lpf_debug_local_register_null_memreg.cpp
    func_lpf_debug_put_after_deregister_dest_after_sync.cpp
    func_lpf_debug_put_after_deregister_dest.cpp
    func_lpf_debug_put_after_deregister_source_after_sync.cpp
    func_lpf_debug_put_after_deregister_source.cpp
    func_lpf_debug_put_get_too_many_requests.cpp
    func_lpf_debug_put_get_too_many_requests_remote.cpp
    func_lpf_debug_put_local_dest_slot.cpp
    func_lpf_debug_put_overflow_dst_offset.cpp
    func_lpf_debug_put_overflow_src_offset.cpp
    func_lpf_debug_put_read_past_source_memory_global.cpp
    func_lpf_debug_put_read_past_source_memory_local.cpp
    func_lpf_debug_put_read_write_conflict_among_many.cpp
    func_lpf_debug_put_read_write_conflict.cpp
    func_lpf_debug_put_too_many_requests.cpp
    func_lpf_debug_put_too_many_requests_remote.cpp
    func_lpf_debug_put_too_many_requests_self.cpp
    func_lpf_debug_put_unknown_dest_pid.cpp
    func_lpf_debug_put_unknown_dest_slot.cpp
    func_lpf_debug_put_unknown_source_slot.cpp
    func_lpf_debug_put_write_past_dest_memory_global_known_at_sync.cpp
    func_lpf_debug_put_write_past_dest_memory_global_known_before_sync.cpp
    func_lpf_debug_register_global_dst_unsynced.cpp
    func_lpf_debug_register_global_src_unsynced.cpp
    func_lpf_debug_register_global_unequal.cpp
    func_lpf_debug_rehook_null_f_symbols.cpp
    func_lpf_debug_rehook_null_input.cpp
    func_lpf_debug_rehook_null_output.cpp
    func_lpf_debug_rehook_null_spmd.cpp
    func_lpf_debug_resize_memory_register_with_size_max_minus_three.cpp
    )


set(LPF_IMPL_CONFIG ${LPFLIB_CONFIG_NAME})
set(debug ON)
set(LPF_IMPL_CONFIG ${LPFLIB_CONFIG_NAME})
set(mode)
if (debug)
    set(mode "_debug")
endif(debug)
foreach (LPF_IMPL_ID ${ENGINES})
    foreach(testSource ${debug_test_sources})

        string(REGEX REPLACE "(.${LPF_IMPL_ID})?.cpp$" "" baseName ${testSource})
        get_filename_component(baseName ${testSource} NAME_WE  )
        set(exeName "${baseName}_${LPF_IMPL_ID}_${LPF_IMPL_CONFIG}${mode}")

        add_gtest(${exeName} ${LPF_IMPL_ID} ${debug} "${CMAKE_CURRENT_SOURCE_DIR}/${testSource}" )

        string(REGEX REPLACE "(.${LPF_IMPL_ID})?.cpp$" "" baseName ${CMAKE_CURRENT_SOURCE_DIR}/${testSource})
        get_filename_component(baseName ${testSource} NAME_WE  )
        set(exeName "${baseName}_${LPF_IMPL_ID}_${LPF_IMPL_CONFIG}${mode}")

    endforeach(testSource)
endforeach(LPF_IMPL_ID)

