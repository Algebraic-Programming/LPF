
/*
 *   Copyright 2021 Huawei Technologies Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <lpf/core.h>
#include <lpf/noc.h>

#include <limits.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>

const lpf_err_t LPF_SUCCESS = 0;
const lpf_err_t LPF_ERR_OUT_OF_MEMORY = 1;
const lpf_err_t LPF_ERR_FATAL = 2;

const lpf_init_t LPF_INIT_NONE = NULL;

const lpf_args_t LPF_NO_ARGS = { NULL, 0, NULL, 0, NULL, 0 };

const lpf_sync_attr_t LPF_SYNC_DEFAULT = 0;

const lpf_msg_attr_t LPF_MSG_DEFAULT = 0;

const lpf_pid_t LPF_MAX_P = UINT_MAX;

const lpf_memslot_t LPF_INVALID_MEMSLOT = SIZE_MAX;

const lpf_machine_t LPF_INVALID_MACHINE = { 0, 0, NULL, NULL };

const lpf_t LPF_NONE = NULL;
const lpf_t LPF_ROOT = "ROOT";

lpf_err_t lpf_hook( lpf_init_t _init, lpf_spmd_t spmd, lpf_args_t args)
{
    (void) _init;
    (*spmd)( LPF_ROOT, 0, 1, args );
    return LPF_SUCCESS;
}

lpf_err_t lpf_rehook( lpf_t ctx, lpf_spmd_t spmd, lpf_args_t args)
{
    (*spmd)( ctx, 0, 1, args );
    return LPF_SUCCESS;
}

lpf_err_t lpf_exec( lpf_t ctx, lpf_pid_t P, lpf_spmd_t spmd, lpf_args_t args)
{
    (void) P;
    (*spmd)( ctx, 0, 1, args );
    return LPF_SUCCESS;
}

lpf_err_t lpf_register_global( lpf_t lpf, void * pointer, size_t size, 
        lpf_memslot_t * memslot )
{
    (void) lpf;
    (void) size;
    char * null = NULL;
    *memslot = (char *) pointer - null;//lint !e413 Intentional use of NULL
    return LPF_SUCCESS;
}

lpf_err_t lpf_register_local( lpf_t lpf, void * pointer, size_t size, 
        lpf_memslot_t * memslot )
{
    (void) lpf;
    (void) size;
    char * null = NULL;
    *memslot = (char * ) pointer - null;//lint !e413 Intentional use of NULL
    return LPF_SUCCESS;
}

lpf_err_t lpf_deregister( lpf_t lpf, lpf_memslot_t memslot )
{
    (void) lpf;
    (void) memslot;
    return LPF_SUCCESS;
}

lpf_err_t lpf_put( lpf_t lpf,
    lpf_memslot_t src_slot, 
    size_t src_offset,
    lpf_pid_t dst_pid, 
    lpf_memslot_t dst_slot, 
    size_t dst_offset, 
    size_t size, 
    lpf_msg_attr_t attr
)
{
    (void) lpf;
    (void) attr;
    (void) dst_pid;
    char * null = NULL;
    char * src_ptr = null + src_slot;//lint !e413 Intentional use of NULL
    char * dst_ptr = null + dst_slot;//lint !e413 Intentional use of NULL
    memcpy( dst_ptr + dst_offset, src_ptr + src_offset, size );
    return LPF_SUCCESS;
}

lpf_err_t lpf_get(
    lpf_t lpf, 
    lpf_pid_t src_pid, 
    lpf_memslot_t src_slot, 
    size_t src_offset, 
    lpf_memslot_t dst_slot, 
    lpf_memslot_t dst_offset,
    size_t size,
    lpf_msg_attr_t attr
){
    (void) lpf;
    (void) attr;
    (void) src_pid;
    char * null = NULL;
    char * src_ptr = null + src_slot;//lint !e413 Intentional use of NULL
    char * dst_ptr = null + dst_slot;//lint !e413 Intentional use of NULL
    memcpy( dst_ptr + dst_offset, src_ptr + src_offset, size );
    return LPF_SUCCESS;
}

lpf_err_t lpf_sync( lpf_t lpf, lpf_sync_attr_t attr )
{
    (void) lpf;
    (void) attr; 
    return LPF_SUCCESS;
}

lpf_err_t lpf_counting_sync_per_slot( lpf_t lpf, lpf_sync_attr_t attr, lpf_memslot_t slot, size_t expected_sent, size_t expected_rcvd)
{
    (void) lpf;
    (void) attr; 
    (void) slot;
    (void) expected_sent;
    (void) expected_rcvd;
    return LPF_SUCCESS;
}

lpf_err_t lpf_lock_slot(
    lpf_t ctx,
    lpf_memslot_t src_slot,
    size_t src_offset,
    lpf_pid_t dst_pid,
    lpf_memslot_t dst_slot,
    size_t dst_offset,
    size_t size,
    lpf_msg_attr_t attr
) {

    (void) ctx;
    (void) src_slot;
    (void) src_offset;
    (void) dst_pid;
    (void) dst_slot;
    (void) dst_offset;
    (void) size;
    (void) attr;
	return LPF_SUCCESS;
}

lpf_err_t lpf_unlock_slot(
    lpf_t ctx,
    lpf_memslot_t src_slot,
    size_t src_offset,
    lpf_pid_t dst_pid,
    lpf_memslot_t dst_slot,
    size_t dst_offset,
    size_t size,
    lpf_msg_attr_t attr
) {
    (void) ctx;
    (void) src_slot;
    (void) src_offset;
    (void) dst_pid;
    (void) dst_slot;
    (void) dst_offset;
    (void) size;
    (void) attr;
	return LPF_SUCCESS;
}

static double messageGap( lpf_pid_t p, size_t min_msg_size, lpf_sync_attr_t attr)
{ 
    (void) p;
    (void) min_msg_size;
    (void) attr;
    return 0.0; 
}

static double latency( lpf_pid_t p, size_t min_msg_size, lpf_sync_attr_t attr)
{ 
    (void) p;
    (void) min_msg_size;
    (void) attr;
    return 0.0; 
}

lpf_err_t lpf_probe( lpf_t lpf, lpf_machine_t * params )
{
    (void) lpf;

    params->p = 1;
    params->free_p = 1;
    params->g = messageGap;
    params->l = latency;

    return LPF_SUCCESS;
}

lpf_err_t lpf_resize_message_queue( lpf_t lpf, size_t max_msgs )
{
    (void) lpf;
    (void) max_msgs;
    return LPF_SUCCESS;
}

lpf_err_t lpf_resize_memory_register( lpf_t lpf, size_t max_regs )
{
    (void) lpf;
    (void) max_regs ;
    return LPF_SUCCESS;
}

lpf_err_t lpf_get_rcvd_msg_count_per_slot( lpf_t lpf, size_t * rcvd_msgs, lpf_memslot_t slot) {
    (void) lpf;
    *rcvd_msgs = 0;
    (void) slot;
    return LPF_SUCCESS;
}

lpf_err_t lpf_get_rcvd_msg_count( lpf_t lpf, size_t * rcvd_msgs) {
    (void) lpf;
    *rcvd_msgs = 0;
    return LPF_SUCCESS;
}

lpf_err_t lpf_get_sent_msg_count_per_slot( lpf_t lpf, size_t * sent_msgs, lpf_memslot_t slot) {
    (void) lpf;
    *sent_msgs = 0;
    (void) slot;
    return LPF_SUCCESS;
}

lpf_err_t lpf_flush( lpf_t lpf) {
    (void) lpf;
    return LPF_SUCCESS;
}

lpf_err_t lpf_abort( lpf_t lpf)
{
    (void) lpf;
    return LPF_SUCCESS;
}

lpf_err_t lpf_noc_resize_memory_register( lpf_t ctx, size_t max_regs ) 
{
    (void) ctx;
    (void) max_regs;
    return LPF_SUCCESS;
}

lpf_err_t lpf_noc_register(
    lpf_t ctx,
    void * pointer,
    size_t size,
    lpf_memslot_t * memslot
) 
{
    (void) ctx;
    (void) pointer;
    (void) size;
    (void) memslot;
    return LPF_SUCCESS;
}

lpf_err_t lpf_noc_deregister(
    lpf_t ctx,
    lpf_memslot_t memslot
) 
{
    (void) ctx;
    (void) memslot;
    return LPF_SUCCESS;
}

lpf_err_t lpf_noc_put(
    lpf_t ctx,
    lpf_memslot_t src_slot,
    size_t src_offset,
    lpf_pid_t dst_pid,
    lpf_memslot_t dst_slot,
    size_t dst_offset,
    size_t size,
    lpf_msg_attr_t attr
)
{
    (void) ctx;
    (void) src_slot;
    (void) src_offset;
    (void) dst_pid;
    (void) dst_slot;
    (void) dst_offset;
    (void) size;
    (void) attr;
    return LPF_SUCCESS;
}

lpf_err_t lpf_noc_get(
    lpf_t ctx,
    lpf_pid_t src_pid,
    lpf_memslot_t src_slot,
    size_t src_offset,
    lpf_memslot_t dst_slot,
    size_t dst_offset,
    size_t size,
    lpf_msg_attr_t attr
)
{
    (void) ctx;
    (void) src_pid;
    (void) src_slot;
    (void) src_offset;
    (void) dst_slot;
    (void) dst_offset;
    (void) size;
    (void) attr;

    return LPF_SUCCESS;
}
