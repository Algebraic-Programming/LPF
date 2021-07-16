
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

#ifndef LPFLIB_HYBRID_H
#define LPFLIB_HYBRID_H

#include <lpf/core.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \addtogroup LPF_EXTENSIONS LPF API extensions
 *
 * @{
 *
 * \defgroup LPF_HYBRID Specific to Hybrid implementation
 *
 * @{
 */


/**
 * Initialises a #lpf_init_t object with LPF contexts from the Pthread
 * implementation and an MPI-RMA implementation. This function must be
 * called collectively by all threads that are in the Pthread and the
 * MPI-RMA contexts.
 *
 * \param[in] thread The Pthread LPF context
 * \param[in] mpi    The MPI-RMA LPF context
 * \param[in] threadId The pid in the Pthread context
 * \param[in] nThreads The number of threads in the Pthread context
 * \param[in] nodeId  The pid in the MPI-RMA context
 * \param[in] nNodes the number of processes in the MPI-RMA context.
 * \param[out] init The #lpf_init_t object that can be used through
 *                     lpf_hook().
 *
 * \returns #LPF_SUCCESS
 *              when the initialisation was successful.
 *
 * \returns #LPF_ERR_OUT_OF_MEMORY
 *              when there wasn't enough memory.
 *
 * \returns #LPF_ERR_FATAL
 *              when the system encountered an unrecoverable error.
 * */
extern _LPFLIB_API
lpf_err_t lpf_hybrid_initialize(
        LPF_RENAME_PRIMITIVE4( lpf, pthread, LPF_CORE_IMPL_CONFIG, _t) thread,
        LPF_RENAME_PRIMITIVE4( lpf, mpimsg, LPF_CORE_IMPL_CONFIG, _t) mpi,
        lpf_pid_t threadId, lpf_pid_t nThreads,
        lpf_pid_t nodeId, lpf_pid_t nNodes, lpf_init_t * init );



/* Releases all resources associated with a #lpf_init_t object that was initialised using
 * lpf_hybrid_initialize(). This function must be called collectively by all
 * processes that participated in the initialization.

 * \param[in] init The #lpf_init_t object that was created through
 *                    lpf_hybrid_intialize().
 *
 * \returns #LPF_SUCCESS
 *              when resources were  successfully released.
 *
 * \returns #LPF_ERR_FATAL
 *              when the system encountered an unrecoverable error.
 */
extern _LPFLIB_API
lpf_err_t lpf_hybrid_finalize( lpf_init_t init );

/**
 * @}
 *
 * @}
 *
 */

#ifdef __cplusplus
}
#endif

#endif
