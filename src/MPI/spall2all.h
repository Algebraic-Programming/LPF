
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

#ifndef LPF_CORE_MPI_SPALL2ALL_H
#define LPF_CORE_MPI_SPALL2ALL_H

#include <stddef.h>
#include <stdint.h>

#include <mpi.h>

#include "rng.h"
#include "linkage.hpp"

#ifdef __cplusplus
extern "C" {
#endif



typedef struct spt_buf_header spt_buf_header_t;
typedef struct spt_msg_header spt_msg_header_t;

typedef struct sparse_all_to_all {
    int * votes, *ballots;
    int vote_cats;
    rng_state_t rng;
    char * heap;
    char * buf[4];
    size_t buf_size;
    spt_buf_header_t *buf_header[4];
    unsigned long *count_buffer;
    int pid, nprocs;
    size_t max_msg_size;
} sparse_all_to_all_t;

/** Create an empty communication buffer for a sparse all-to-all, which is
 *  optimized for cases where there is little data to send and whose
 *  distribution is highly imbalanced. A subsequent call to 
 *  sparse_all_to_all_reserve() is required before it can be used
 *
 * \param obj       A pointer to an empty #sparse_all_to_all_t object.
 * \param pid       The process identifier of this process
 * \param nprocs    The number of processes
 * \param rng_seed  The seed for a random number generator
 * \param vote_cats The number of voting categories
 * \param max_msg_size The maximum size of an MPI message (<= INT_MAX)
 *
 */
_LPFLIB_LOCAL
void sparse_all_to_all_create( sparse_all_to_all_t * obj, 
        int pid, int nprocs, uint64_t rng_seed, int vote_cats,
        size_t max_msg_size );

/** Set the capacity of the communication buffer for a sparse all-to-all to
 * \a buf_size in bytes divided over a maximum of \a n_msgs number of
 * messages. 
 *
 * \param obj       A pointer to an empty #sparse_all_to_all_t object.
 * \param buf_size  The maximum total number of bytes of payload data to send or
 *                  receive per process.
 * \param n_msgs    The maximum number of messages that can be sent or received.
 *
 * \returns 0 on success. In case of an error, e.g. insufficient memory, returns
 * a non-zero value.
 */
_LPFLIB_LOCAL
int sparse_all_to_all_reserve( sparse_all_to_all_t * obj, 
        size_t buf_size, size_t n_msgs );

/** Releases resources that are held by a sparse all-to-all communication
 * buffer. */
_LPFLIB_LOCAL
void sparse_all_to_all_destroy( sparse_all_to_all_t * obj );


/** Copies contents from the sparse all-to-all buffer \a orig to another 
 * sparse all-to-all buffer \a copy. The \a copy buffer must have been 
 * created previously with sparse_all_to_all_create().
 *
 * \param orig The original sparse all-to-all buffer
 * \param copy The destination buffer
 * \returns 0 on success. In case of an error, e.g. the destination buffer is to
 * small, returns a non-zero value.
 */
_LPFLIB_LOCAL
int sparse_all_to_all_copy( const sparse_all_to_all_t * orig, sparse_all_to_all_t * copy);

/** Clears the contents of the sparse all-to-all buffer. */
_LPFLIB_LOCAL
void sparse_all_to_all_clear( sparse_all_to_all_t * obj );

/** Get the total number of messages that can be sent, assuming all 
 * messages take \a message_size payload. */
_LPFLIB_LOCAL
size_t sparse_all_to_all_get_buf_capacity( const sparse_all_to_all_t * obj, 
        size_t messages_size);

/** Returns whether there are any messages in the queue. */
_LPFLIB_LOCAL
int sparse_all_to_all_is_empty( const sparse_all_to_all_t * obj);

/** Take a copy of [\a msg , \a msg + \a size ) and enqueue that as message
 * to specified destination \a dest_pid. Note that messages don't have to
 * be of fixed size, although that makes in-place indexing impossible.
 *
 * \param obj      The sparse all-to-all object
 * \param dest_pid The pid of the destination process 
 * \param msg      The start of the contents of message 
 * \param size     The size of the message in bytes
 *
 * \returns 0 on success. If not enough space had been allocated in the buffer,
 * e.g. by sparse_all_to_all_create, a non-zero value is returned.
 * */
_LPFLIB_LOCAL
int sparse_all_to_all_send( sparse_all_to_all_t * obj,
        int dest_pid, const char * msg, size_t size);

/** If there is a message in the queue, compares its size with <tt>*size</tt>,
 * and assign <tt>*size</tt> the message size. If the <tt>*size</tt> was
 * larger, it copies the contents of the message to \a msg, removes it, and
 * returns 0.  Otherwise, it returns a non-zero value.
 *
 * \param obj      The sparse all-to-all object
 * \param msg      The start of the buffer to hold the message
 * \param size     A pointer to the number which is to hold the size of the
 *                 message
 *
 * \returns 0 on success. If the queue was empty or when the target buffer
 * was too small, returns a non-zero value.
 * */
_LPFLIB_LOCAL
int sparse_all_to_all_recv( sparse_all_to_all_t * obj,
        char * msg, size_t * size);

/** Execute the sparse all-to-all with or without prerandomization. Without
 * prerandomization the buffer size is required to be \f$ h \sqrt{p} \f$
 * and \f$ p \f$ must be a power of 2, where \f$ h \f$ is the maximum number of
 * bytes that any process receives or send and \f$ p \f$ is the number of
 * processes. With prerandomization $\f p \f$ can be any positive non-zero
 * value, while the the required buffer space need only to be 
 * \f$ h (1+log(p)) \f$ to allow the all-to-all to finish successfully with
 * high probability. 
 *
 * As a bonus this function computes the elementwise sum of \a vote over all
 * processes.  This is useful for error handling in code that uses this
 * function.
 * 
 * \param comm The MPI communicator
 * \param obj  The sparse all-to-all buffer
 * \param do_prerandomize Whether or not to prerandomize.
 * \param vote An array of votes, one for each category.
 * \param ballot The ballot for each of the categories.
 *
 * \returns Returns a negative number in case  of failure.
 * */
_LPFLIB_LOCAL
int sparse_all_to_all( MPI_Comm comm, 
        sparse_all_to_all_t * obj, int do_prerandomize,
        const int * vote, int * ballot );


#ifdef __cplusplus
}  // extern "C"
#endif

#endif

