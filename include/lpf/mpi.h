
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

#ifndef LPFLIB_MPI_H
#define LPFLIB_MPI_H

#include <lpf/core.h>
#include <mpi.h>

#ifdef __cplusplus
extern "C" {     
#endif

/** \addtogroup LPF_EXTENSIONS LPF API extensions
 *
 * @{
 *
 * \defgroup LPF_MPI Specific to implementations based on MPI
 *
 * @{
 */

/**
 * Whether the LPF library should automatically initialize itself. If so,
 * it calls MPI_Init() itself and pauses the processes with PID != 0 until
 * a call to lpf_exec(). When disabled, the user code should call
 * MPI_Init() and arrange for processes to call either
 * lpf_mpi_initialize_with_mpicomm() or lpf_mpi_initialize_over_tcp(),
 * followed by lpf_hook(). 
 *
 * By default, the LPF library will auto-initialise. When launching an LPF
 * program binary, this behaviour can be overridden via two mechanisms:
 *  -# launching a binary that defines this field with the value zero;
 *  -# launching a binary while using the -no-auto-init flag to lpfrun.
 *
 * The former works by the LPF library weak-linking the default symbol that
 * holds the default value of one. The latter mechanism skips the check for
 * this symbol altogether, and assumes auto-initialisation is never required.
 *
 * \par Rationale
 * The setting it represents must be known to the implementation before the
 * program starts, because it decides whether MPI processes with \a rank > 0
 * enter a wait-loop instead of main(). So, this rules out a function call to
 * modify this behaviour.
 * Additionally, the operation of this setting is strongly coupled with how the
 * program text is structured: "Is it going to use lpf_hook() or lpf_exec()?".
 * This disqualifies the use of an environment variable. Therefore, a constant
 * with static storage is chosen. By letting the implementation define it as a
 * weak symbol equal to one, the normal way to start a program, through
 * lpf_exec(), is enabled by default.
 * Some cases may preclude modification of the binary given to lpfrun. If,
 * additionally, that use case aims for the use of lpf_hook(), a launch-time
 * mechanism for is additionally required.
 */
extern const int LPF_MPI_AUTO_INITIALIZE ;


/** Initialises a #lpf_init_t object with an MPI communicator for use
 * with lpf_hook(). This function must be called collectively by all processes
 * in the MPI communicator. An obvious use for this function is to allow
 * existing MPI applications to use LPF codes.
 *
 * \param[in] comm The MPI communicator
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
lpf_err_t lpf_mpi_initialize_with_mpicomm( MPI_Comm comm, lpf_init_t * init );

/** Initialises a #lpf_init_t object over TCP/IP for use with lpf_hook(). This
 * function must be called collectively by \a nprocs processes. On each process
 * the parameters must be the same, except \a pid which must be a unique number
 * ranging from to 0 to \a nprocs - 1. An obvious use for this function is to
 * allow an arbitrary application to use LPF code.
 *
 * \param[in] server The Internet host that will execute this function 
 *                   with \a pid = 0.
 * \param[in] port   The port number that the server will use.                  
 * \param[in] timeout Maximum number of milliseconds allowed to establish
 *                    connection. If any of the clients fails to connect within
 *                    that timeout, an error is returned and the MPI runtime is
 *                    left in undefined state. <strong>Do not call
 *                    MPI_Finalize() and use _exit() or _Exit() to exit</strong>. 
 * \param[in] pid    The process identifier must be a number in the range 
 *                   from 0 to and inclusive \a nprocs - 1. This number must be
 *                   unique among the processes that calls function. The process
 *                   with \a pid = 0, must be located on the Internet host 
 *                   \a server.
 *
 * \param[in] nprocs The number of processes that shall be part of \a init.
 *
 * \param[out] init The #lpf_init_t object that can be used through
 *                     lpf_hook().
 *
 * \returns #LPF_SUCCESS 
 *              when the initialisation was successful.
 *
 * \returns #LPF_ERR_OUT_OF_MEMORY 
 *              when there wasn't enough memory available.
 *
 * \returns #LPF_ERR_FATAL
 *              when the system encountered an unrecoverable error, such as 
 *              when a timeout occurs or when the underlying MPI implementation
 *              does not support dynamic connections. <strong>Do not call
 *              MPI_Finalize() and use _exit() or _Exit() to exit</strong>.
 */
lpf_err_t lpf_mpi_initialize_over_tcp( 
        const char * server, const char * port, int timeout,
        lpf_pid_t pid, lpf_pid_t nprocs, 
        lpf_init_t * init );


/* Releases all resources associated with a #lpf_init_t object that was initialised using
 * lpf_mpi_initialize_with_mpicomm(). This function must be called collectively by all
 * processes that participated in the initialization. 

 * \param[in] init The #lpf_init_t object that was created through
 *                    lpf_mpi_initialize_with_mpicomm().
 *
 * \returns #LPF_SUCCESS 
 *              when resources were  successfully released.
 *
 * \returns #LPF_ERR_FATAL
 *              when the system encountered an unrecoverable error.
 */
lpf_err_t lpf_mpi_finalize( lpf_init_t init );

lpf_err_t lpf_debug_abort();

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
