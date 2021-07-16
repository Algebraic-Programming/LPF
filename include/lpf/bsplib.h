
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

#ifndef LPFLIB_CORE_LIBRARY_BSPLIB_H
#define LPFLIB_CORE_LIBRARY_BSPLIB_H

#include <lpf/core.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \addtogroup LPF_HL Lightweight Parallel Foundation's higher-level libraries
 *
 * @{
 *
 * \defgroup LPF_BSPLIB BSPlib-like API
 *
 * @{
 *
 */

typedef struct BSPlib * bsplib_t ;

typedef enum bsplib_err { 
    BSPLIB_SUCCESS,
    BSPLIB_ERR_OUT_OF_MEMORY,
    BSPLIB_ERR_MEMORY_NOT_REGISTERED,
    BSPLIB_ERR_PUSHREG_MISMATCH,
    BSPLIB_ERR_POPREG_MISMATCH,
    BSPLIB_ERR_TAGSIZE_MISMATCH,
    BSPLIB_ERR_NULL_POINTER,
    BSPLIB_ERR_PID_OUT_OF_RANGE,
    BSPLIB_ERR_MEMORY_ACCESS_OUT_OF_RANGE,
    BSPLIB_ERR_EMPTY_MESSAGE_QUEUE,
    BSPLIB_ERR_FATAL
} bsplib_err_t;



extern _LPFLIB_API
bsplib_err_t bsplib_create( lpf_t ctx, lpf_pid_t pid, lpf_pid_t nprocs, 
        int safemode, size_t max_hp_regs, bsplib_t * bsplib );

extern _LPFLIB_API
bsplib_err_t bsplib_destroy( bsplib_t bsplib );


extern _LPFLIB_API
double bsplib_time( bsplib_t bsplib );

extern _LPFLIB_API
lpf_pid_t bsplib_nprocs( bsplib_t bsplib );

extern _LPFLIB_API
lpf_pid_t bsplib_pid( bsplib_t bsplib );

extern _LPFLIB_API
bsplib_err_t bsplib_sync( bsplib_t bsplib );

extern _LPFLIB_API
bsplib_err_t bsplib_push_reg( bsplib_t bsplib, const void * ident, size_t size );

extern _LPFLIB_API
bsplib_err_t bsplib_pop_reg( bsplib_t bsplib, const void * ident );

extern _LPFLIB_API
bsplib_err_t bsplib_put( bsplib_t bsplib, lpf_pid_t dst_pid, const void * src,
        void * dst, size_t offset, size_t nbytes );

extern _LPFLIB_API
bsplib_err_t bsplib_hpput( bsplib_t bsplib, lpf_pid_t dst_pid, const void * src,
        void * dst, size_t offset, size_t nbytes );

extern _LPFLIB_API
bsplib_err_t bsplib_get( bsplib_t bsplib, lpf_pid_t src_pid, const void * src,
        size_t offset, void * dst, size_t nbytes );

extern _LPFLIB_API
bsplib_err_t bsplib_hpget( bsplib_t bsplib, lpf_pid_t src_pid, const void * src,
        size_t offset, void * dst, size_t nbytes );

extern _LPFLIB_API
size_t bsplib_set_tagsize( bsplib_t bsplib, size_t tagsize );

extern _LPFLIB_API
bsplib_err_t bsplib_send( bsplib_t bsplib, lpf_pid_t dst_pid, const void * tag,
        const void * payload, size_t nbytes );

extern _LPFLIB_API
bsplib_err_t bsplib_hpsend( bsplib_t bsplib, lpf_pid_t dst_pid, const void * tag,
        const void * payload, size_t nbytes );

extern _LPFLIB_API
bsplib_err_t bsplib_qsize( bsplib_t bsplib, size_t * nmessages, size_t * accum_bytes );

extern _LPFLIB_API
bsplib_err_t bsplib_get_tag( bsplib_t bsplib, size_t * status, void * tag );

extern _LPFLIB_API
bsplib_err_t bsplib_move( bsplib_t bsplib, void * payload, size_t reception_bytes );

extern _LPFLIB_API
size_t bsplib_hpmove( bsplib_t bsplib, const void ** tag_ptr, const void ** payload_ptr );

#ifdef __cplusplus
}
#endif

/**
 * @}
 *
 * @}
 *
 */

#endif

