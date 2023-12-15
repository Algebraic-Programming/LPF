
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

#ifndef LPFLIB_DEBUG_H
#define LPFLIB_DEBUG_H

#include <stddef.h>

#include "../../lpf/core.h"


#ifdef __cplusplus
extern "C" {     
#endif


#define lpf_exec( ctx, P, spmd, args ) \
    lpf_debug_exec( __FILE__, __LINE__, (ctx), (P), (spmd), (args) )

#define lpf_hook( init, spmd, args )\
    lpf_debug_hook( __FILE__, __LINE__, (init), (spmd), (args) )

#define lpf_rehook( ctx, spmd, args )\
    lpf_debug_rehook( __FILE__, __LINE__, (ctx), (spmd), (args) )

#define lpf_register_global( ctx, pointer, size, memslot ) \
    lpf_debug_register_global( __FILE__, __LINE__,\
            (ctx), (pointer), (size), (memslot) )    

#define lpf_register_local( ctx, pointer, size, memslot ) \
    lpf_debug_register_local( __FILE__, __LINE__,\
            (ctx), (pointer), (size), (memslot) )    

#define lpf_deregister( ctx, memslot ) \
    lpf_debug_deregister( __FILE__, __LINE__, (ctx), (memslot) )

#define lpf_put( ctx, src_slot, src_offset, dst_pid, dst_slot, dst_offset, size, attrs ) \
    lpf_debug_put( __FILE__, __LINE__, \
            (ctx), (src_slot), (src_offset), \
            (dst_pid), (dst_slot), (dst_offset), (size), (attrs) )

#define lpf_get( ctx, src_pid, src_slot, src_offset, dst_slot, dst_offset, size, attrs ) \
    lpf_debug_get( __FILE__, __LINE__, \
            (ctx), (src_pid), (src_slot), (src_offset), \
            (dst_slot), (dst_offset), (size), (attrs) )

#define lpf_probe( ctx, machine ) \
    lpf_debug_probe( __FILE__, __LINE__, (ctx), (machine) )

#define lpf_sync( ctx, attrs ) \
    lpf_debug_sync( __FILE__, __LINE__, (ctx), (attrs) )

#define lpf_counting_sync_per_tag( ctx, attrs, slot, expected_sends, expected_rcvs ) \
    lpf_debug_counting_sync_per_tag( __FILE__, __LINE__, (ctx), (attrs), (slot), (expected_sends), (expected_rcvs) )

#define lpf_sync_per_tag( ctx, attrs, slot) \
    lpf_debug_sync_per_tag( __FILE__, __LINE__, (ctx), (attrs), (slot))

#define lpf_resize_memory_register( ctx, size ) \
    lpf_debug_resize_memory_register( __FILE__, __LINE__, (ctx), (size) )

#define lpf_resize_message_queue( ctx, size ) \
    lpf_debug_resize_message_queue( __FILE__, __LINE__, (ctx), (size) )


extern _LPFLIB_API 
lpf_err_t lpf_debug_exec( const char * file, int line,
    lpf_t ctx, lpf_pid_t P, lpf_spmd_t spmd, lpf_args_t args
);

extern _LPFLIB_API 
lpf_err_t lpf_debug_hook( const char * file, int line,
    lpf_init_t init, lpf_spmd_t spmd, lpf_args_t args
);


extern _LPFLIB_API 
lpf_err_t lpf_debug_rehook( const char * file, int line,
    lpf_t ctx,
    lpf_spmd_t spmd,
    lpf_args_t args
);

extern _LPFLIB_API 
lpf_err_t lpf_debug_register_global( const char * file, int line,
    lpf_t ctx, void * pointer, size_t size, lpf_memslot_t * memslot
);

extern _LPFLIB_API 
lpf_err_t lpf_debug_register_local( const char * file, int line,
    lpf_t ctx, void * pointer, size_t size, lpf_memslot_t * memslot
);

extern _LPFLIB_API 
lpf_err_t lpf_debug_deregister( const char * file, int line,
    lpf_t ctx, lpf_memslot_t memslot
);

extern _LPFLIB_API 
lpf_err_t lpf_debug_put( const char * file, int line,
    lpf_t ctx, lpf_memslot_t src_slot, size_t src_offset,
    lpf_pid_t dst_pid, lpf_memslot_t dst_slot, size_t dst_offset,
    size_t size, lpf_msg_attr_t attr
);

extern _LPFLIB_API 
lpf_err_t lpf_debug_get( const char * file, int line,
    lpf_t ctx, lpf_pid_t src_pid, lpf_memslot_t src_slot, size_t src_offset,
    lpf_memslot_t dst_slot, size_t dst_offset, size_t size, lpf_msg_attr_t attr
);

extern _LPFLIB_API 
lpf_err_t lpf_debug_probe( const char * file, int line, 
        lpf_t ctx, lpf_machine_t * params ); 

extern _LPFLIB_API 
lpf_err_t lpf_debug_sync( const char * file, int line, 
        lpf_t ctx, lpf_sync_attr_t attr );

lpf_err_t lpf_debug_counting_sync_per_tag( const char * file, int line, 
        lpf_t ctx, lpf_sync_attr_t attr, lpf_memslot_t slot, size_t expected_sends, size_t expected_rcvs);

extern _LPFLIB_API 
lpf_err_t lpf_debug_resize_memory_register( const char * file, int line,
        lpf_t ctx, size_t max_regs );

extern _LPFLIB_API 
lpf_err_t lpf_debug_resize_message_queue( const char * file, int line,
        lpf_t ctx, size_t max_msgs );

#ifdef __cplusplus
}
#endif


#endif

