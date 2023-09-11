
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

#ifndef LPFLIB_RPC_CLIENT
#define LPFLIB_RPC_CLIENT

#include <lpf/core.h>

#ifdef __cplusplus
extern "C" {     
#endif

/**
 * \addtogroup LPF_HL Lightweight Parallel Foundation's higher-level libraries
 *
 * @{
 *
 * \defgroup LPF_RPC RPC over LPF
 *  
 * Some aspects are implementation defined. See, e.g., #lpf_mpirpc_open.
 * 
 * @{
 */

/**
 * Represents an RPC server connection.
 * 
 * A valid instance of this type can be created via a successful call to
 * e.g. lpf_mpirpc_open(). #LPF_RPC_SERVER_NONE is an invalid initializer
 * constant for this type. A valid instance is freed (and thus made invalid)
 * by placing a call to e.g. lpf_mpirpc_close_server().
 */
typedef lpf_init_t lpf_rpc_server_t;

/**
 * Represents a query that is to be sent over an RPC server connection.
 * 
 * A valid instance of this type can only be created via a successful call to
 * #lpf_rpc_query_create. #LPF_RPC_QUERY_NONE is an invalid initializer constant
 * for this type. A valid instance is freed (and thus made invalid) by placing a
 * call to #lpf_rpc_query_destroy.
 */
typedef lpf_args_t lpf_rpc_query_t;

/** An (invalid) initializer instance of #lpf_rpc_server_t. */
extern _LPFLIB_API const lpf_rpc_server_t LPF_RPC_SERVER_NONE;

/** An (invalid) initializer instance of #lpf_rpc_query_t. */
extern _LPFLIB_API const lpf_rpc_query_t LPF_RPC_QUERY_NONE;

/** Signals a failed RPC called. Retry is possible. */
extern _LPFLIB_API const lpf_err_t LPF_ERR_RPC_FAILED;

/**
 * Takes a user's payload for an RPC server query and reserves room for the
 * server's response.
 * 
 * @param[in]  payload       The query contents.
 * @param[in]  payload_size  The size (in bytes) of the query payload.
 * @param[in]  response_size The (maximum) size (in bytes) the server can
 *                           return in response to the query.
 * @param[out] query         Where to store the resulting query object.
 *
 * \note It is allowed for \a payload to be equal to \a NULL and
 *       \a payload_size to be equal to zero (as long as the server knows how to
 *       interpret such an `empty' request, of course).
 * 
 * \note Likewise, it is also allowed for \a response_size to be empty.
 * 
 * @returns #LPF_ERR_OUT_OF_MEMORY If there is not enough memory available to
 *                                 construct \a query. In this case it will be
 *                                 as though the call to this function has
 *                                 never happened (other than returning this
 *                                 exit code).
 * 
 * @returns #LPF_SUCCESS On successful creation of the query.
 */
extern _LPFLIB_API
lpf_err_t lpf_rpc_query_create(
    const void * const payload,
    const size_t payload_size,
    const size_t response_size,
    lpf_rpc_query_t * const query
);

/**
 * Releases all resources corresponding to a valid instance of type
 * #lpf_rpc_query_t.
 * 
 * @param[in] query The query which to destroy. This must be a valid instance
 *                  of type #lpf_rpc_query_t.
 * 
 * On function exit, \a query shall be invalid.
 * 
 * @returns LPF_SUCCESS This function always succeeds.
 * 
 * \warning This function's behaviour is undefined when an invalid \a query
 *          is provided.
 */
extern _LPFLIB_API
lpf_err_t lpf_rpc_query_destroy( lpf_rpc_query_t query );

/**
 * Execute the query at the given RPC server.
 *
 * @param[in] query       A valid query instance.
 * @param[in] server      A valid server connection.
 * @param[out] response   Where to store a pointer to the memory area which
 *                        contains the server's response. This memory area will
 *                        be guaranteed valid until the \a query is destroyed,
 *                        overwritten, or subject to a new call to this
 *                        function.
 * 
 * \warning Passing an invalid query or connection will result in UB.
 *
 * \note After a successful call to this function, the same query remains valid
 *       and can thus be re-submitted to the server to obtain new responses.
 *       Whether such new responses can differ from each other is application-
 *       defined.
 * 
 * @returns #LPF_SUCCESS When the query successfully executed and an answer
 *                       (if requested) was sent back successfully as well.
 * 
 * @returns #LPF_ERR_RPC_FAILED A non-fatal error occurred while communicating
 *                              with the RPC server or the RPC server returned
 *                              an error. The \a response output will not be
 *                              set. Simply retrying the query is allowed but
 *                              its effectiveness is application-defined.
 *  
 * @returns #LPF_ERR_OUT_OF_MEMORY When the connection or query failed due to
 *                                 out of memory conditions.
 *
 * @returns #LPF_ERR_FATAL In case a fatal error occurred during querying. In
 *                         this case, the LPF enters an undefined state and
 *                         should no longer be relied on during the remaining
 *                         life time of this RPC client process.
 */
extern _LPFLIB_API
lpf_err_t lpf_rpc_send_query( lpf_rpc_query_t query, lpf_rpc_server_t server, void * * const response );

/**  @} */
/** @} */

#ifdef __cplusplus
}
#endif

#endif //end ``LPFLIB_RPC_CLIENT''

