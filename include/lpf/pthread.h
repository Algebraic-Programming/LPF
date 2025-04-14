
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

#ifndef LPFLIB_PTHREAD_H
#define LPFLIB_PTHREAD_H

#include "lpf/core.h"

#ifdef __cplusplus
extern "C" {     
#endif

/** \addtogroup LPF_EXTENSIONS LPF API extensions
 *
 * @{
 *
 * \defgroup LPF_PTHREAD Specific to the Pthreads engine
 *
 * @{
 */

/** When called from within a multi-threaded program, it initialises the
 * #lpf_init_t object for use with lpf_hook(). All threads that are supposed
 * to join in the lpf_hook() are required to call this function collectively with a 
 * private memory location for \a init .  An obvious use for this function is
 * to allow existing multi-threaded applications to use LPF codes.
 *
 * \param[in] pid The same id that this thread will get when lpf_hook() is
 *                called.
 * \param[in] nprocs The number of threads that call this function and the
 *                subsequent call to lpf_hook().
 *
 * \param[out] init The #lpf_init_t init object that can be used through
 *                     lpf_hook(). The memory location for this variable can but
 *                     is not required to be shared between threads.
 *
 * \returns #LPF_SUCCESS 
 *              at successful initialization.
 *
 * \returns #LPF_ERR_OUT_OF_MEMORY 
 *              when there wasn't enough memory.
 *
 * \returns #LPF_ERR_FATAL
 *              when the system encountered an unrecoverable error.
 *
 * */
extern _LPFLIB_API
lpf_err_t lpf_pthread_initialize( lpf_pid_t pid, lpf_pid_t nprocs,
        lpf_init_t * init );

/* Releases all resources associated with a #lpf_init_t object that was created
 * by lpf_pthread_initialize(). This function must be called collectively by
 * all threads that took part in the initialization.
 *
 * \param[in] init The #lpf_init_t object that was created through
 *                    lpf_pthread_initialize().
 *
 * \returns #LPF_SUCCESS 
 *              when all resource were successfully released.
 *
 * \returns #LPF_ERR_FATAL
 *              when the system encountered an unrecoverable error.
 */
extern _LPFLIB_API
lpf_err_t lpf_pthread_finalize( lpf_init_t init );

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
