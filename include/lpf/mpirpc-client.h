
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

#ifndef LPFLIB_MPIRPC_CLIENT
#define LPFLIB_MPIRPC_CLIENT

#include <lpf/rpc-client.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \addtogroup LPF_HL Lightweight Parallel Foundation's higher-level libraries
 *
 * @{
 *
 * \addtogroup LPF_RPC RPC over LPF
 *  
 * @{
 */

/**
 * Opens a connection to an RPC server.
 * 
 * @param[in]  hostname  The name of the host to connect to.
 * @param[in]  portname  The name of the port to connect to.
 * @param[in]  timeout   How long to wait for a successful connection (in ms.)
 * @param[out] server    The open connection to the RPC server.
 *
 * @returns #LPF_SUCCESS If the connection was opened successfully.
 *
 * @returns #LPF_SUCCESS If \a hostname or \a portname was empty, or if
 *                       \a timeout was zero. In either of these cases,
 *                       the call to this function shall have no other effect
 *                       than returning SUCCESS.
 *
 * @returns #LPF_ERR_OUT_OF_MEMORY In case not enough memory was available to
 *                                 successfully open up a connection.
 *
 * @returns #LPF_ERR_FATAL In case the connection failed. In this case, the
 *                         LPF enters an undefined state and should no longer
 *                         be relied on during the remaining life time of this
 *                         RPC client process.
 */
extern _LPFLIB_API
lpf_err_t lpf_mpirpc_open(
    const char * const hostname, const char * const portname,
    const int timeout,
    lpf_rpc_server_t * const server
);

/**
 * Closes an RPC server connection.
 * 
 * @param[in] server A \em valid server connection created using a (successful)
 *                   call to #lpf_mpirpc_open.
 *
 * \warning Supplying an invalid connection will yield undefined behaviour.
 * 
 * @returns #LPF_SUCCESS   When the connection is successfully closed.
 * 
 * @returns #LPF_ERR_FATAL In case closing the connection failed. In this case,
 *                         the LPF enters an undefined state and should no longer
 *                         be relied on during the remaining life time of this
 *                         RPC client process.
 */
extern _LPFLIB_API
lpf_err_t lpf_mpirpc_close_server( const lpf_rpc_server_t server );

/**  @} */
/** @} */

#ifdef __cplusplus
}
#endif

#endif //end ``LPFLIB_MPIRPC_CLIENT''

