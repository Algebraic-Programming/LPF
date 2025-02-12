
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

#ifndef _H_LPF_COLLECTIVES
#define _H_LPF_COLLECTIVES

#include <lpf/core.h>
#include <stdbool.h>


#ifdef __cplusplus
#define restrict __restrict__
extern "C" {
#endif

/**
 * \addtogroup LPF_HL Lightweight Parallel Foundation's higher-level libraries
 * 
 * @{
 *
 * \defgroup LPF_COLLECTIVES LPF Collectives API 
 *
 * @{
 */

/**
 * The version of this collectives specification.
 */
#define _LPF_COLLECTIVES_VERSION 201500L

/** The run-time state of a LPF collectives library communicator. */
typedef struct lpf_coll {
	/** The LPF context in which this #lpf_coll_t object was created. */
	lpf_t ctx;

	/** Inverse map {0,...,p-1} -> {0,...,coll.P-1} in case of a strided collective. */
	lpf_pid_t * inv_translate;

	/** Map {0,...,coll.P-1} -> {0,...,p-1} in cases of a strided collective. */
	lpf_pid_t * translate;

	/** Buffer provisioned primarily for use with #lpf_reduce and #lpf_allreduce. */
	void * buffer;

	/** Memory slot corresponding to the #lpf_coll_t.buffer. */
	lpf_memslot_t slot;

	/** The number of processes involved with this #lpf_coll_t. */
	lpf_pid_t P;

	/** The process ID within this #lpf_coll_t. May differ from the global s. */
	lpf_pid_t s;
} lpf_coll_t;

/**
 * A type of a reducer used with lpf_reduce() and lpf_allreduce(). This
 * function defines how an \a array of \a n elements should be reduced into a
 * single \a value. The value is assumed to be initialised at function entry.
 * 
 * \param[in] n The number of elements in the \a array.
 * \param[in] array The array to reduce.
 * \param[in,out] value An initial reduction value into which all values in the
 *                      \a array are reduced.
 *
 * This is a user-defined function that the LPF collectives library makes
 * callbacks to. There are no hard guarantees on the runtime incurred during
 * the induced computation phases, nor are there any guarantees on correctness
 * and failure mitigation.
 */
typedef void (*lpf_reducer_t) (size_t n, const void * array, void * value);

/**
 * A type of combiner used with lpf_combine() and lpf_allcombine(). This
 * function defines how an \a array of \a n elements should by combined with
 * an existing array of the same size. All arrays should be assumed to be
 * fully initialised at function entry.
 *
 * A #lpf_combiner_t function may not be applied on the full input array; both
 * arrays of size \a N may be cut into smaller pieces by the LPF collectives
 * library, which then calls this #lpf_combiner_t function repeatedly on each
 * of the smaller pieces of both arrays.
 *
 * For consistent results, the #lpf_combiner_t should be associative and 
 * commutative. Other reduces should only be used with greater thought and
 * full understanding of the underlying collective algorithms, which, of
 * course, are implementation dependent.
 * 
 * \warning The parameter \a n is not a byte size unless the elements of the
 *          arrays happen to be a single byte large.
 *
 * \param[in] n        The number of elements in both the \a combine and
 *                     \a into arrays.
 * \param[in] combine  The data to be combined the destination array.
 * \param[in,out] into An array with existing data into which to combine
 *                     the array \a combine.
 */
typedef void (*lpf_combiner_t) (size_t n, const void * combine, void * into );

/**
 * An invalid #lpf_coll_t. Can be used as a static initialiser, but can never
 * be used as input to any LPF collective as its contents are invalid.
 */
extern _LPFLIB_API const lpf_coll_t LPF_INVALID_COLL;

/**
 * ToDo: document allgatherv
 */
lpf_err_t lpf_allgatherv(
        lpf_coll_t coll,
        lpf_memslot_t src,
        lpf_memslot_t dst,
        size_t *sizes, 
        bool exclude_myself
        );
/**
 * Initialises a collectives struct, which allows the scheduling of collective
 * calls. The initialised struct is only valid after a next call to lpf_sync().
 * 
 * All operations using the output argument \a coll need to be made
 * \em collectively; i.e., all processes must make the same function call with
 * possibly different arguments, as prescribed by the collective in question.
 *
 * \param[in,out] ctx     The LPF runtime state as provided by lpf_exec() or
 *                        lpf_hook() via #lpf_spmd_t.
 * \param[in]     s       The unique ID number of this process in this LPF SPMD
 *                        run. Must be larger or equal to zero; may not be
 *                        larger or equal to \a p.
 * \param[in]     p       The number of processes involved with this LPF SPMD
 *                        run. Must be larger or equal to zero. May not be
 *                        larger than the value provided by lpf_exec() or
 *                        lpf_hook() via #lpf_spmd_t.
 * \param[in] max_calls   The number of collective calls a single LPF process
 *                        may make within a single superstep. Must be larger
 *                        than zero.
 * \param[in] max_elem_size The maximum number of bytes of a single element to
 *                          be reduced through #lpf_reduce or #lpf_allreduce.
 *                          Must be larger or equal to zero.
 * \param[in] max_byte_size The maximum number of bytes given as a \a size
 *                          parameter to #lpf_broadcast, #lpf_scatter,
 *                          #lpf_gather, #lpf_allgather, or #lpf_alltoall. Must
 *                          be larger or equal to zero. For the #lpf_combine
 *                          and #lpf_allcombine collectives, the effective byte
 *                          size is \f$ \mathit{num}\mathit{size} \f$.
 * \param[out] coll         The collective struct needed to perform collective
 *                          operations using this library. After a successful
 *                          call to this function, this return #lpf_coll_t
 *                          value may be used immediately.
 *
 * <em>This implementation will register one memory slot on every call to this
 *     function.</em>
 * 
 * \return LPF_ERR_OUT_OF_MEMORY When the requested buffers would cause the
 *                               system to go out of memory. After returning
 *                               with this error code, it shall be as though
 *                               the call to this function had not occurred.
 *
 * \return LPF_SUCCESS  When the function executed successfully.
 */
extern _LPFLIB_API
lpf_err_t lpf_collectives_init(
	lpf_t ctx,
	lpf_pid_t s,
	lpf_pid_t p,
	size_t max_calls,
	size_t max_elem_size,
	size_t max_byte_size,
	lpf_coll_t * coll
);

/**
 * Returns the context embedded within a collectives instance.
 *
 * \param[in] coll A valid collectives instance.
 *
 * \return The LPF core API context used to construct \a coll with.
 *
 * \see lpf_collectives_init
 *
 * \warning Calling this function using an invalid \a coll will result in
 *          undefined behaviour.
 */
extern _LPFLIB_API
lpf_t lpf_collectives_get_context( lpf_coll_t coll );

/**
 * Initialises a collectives struct, which allows the scheduling of collective
 * calls. The initialised struct is only valid after a next call to lpf_sync().
 * This variant selects only a subset of processes based on a lower PID bound
 * (inclusive), and upper bound (exclusive), and a stride.
 *
 * All operations using the output argument \a coll need to be made
 * \em collectively; i.e., all processes that are involved with \a coll must
 * make the same function call with possibly different arguments, as prescribed
 * by the collective in question.
 * 
 * \note Thus not all processes need to make the same collective call. If,
 *       e.g., a range from 10 to 20 with stride 2 is used to construct \a coll
 *       then every collective call at PIDs 10, 12, 14, 16, 18 must be matched
 *       with a similar call at all other processes with PID in that same set.
 *
 * \param[in,out] ctx       The LPF runtime state as provided by lpf_exec() or
 *                          lpf_hook().
 * \param[in]     s         The unique ID number of this process in this LPF
 *                          SPMD run. Must be larger or equal to zero; may not
 *                          be larger or equal to \a p. May not be smaller than
 *                          \a lo.
 * \param[in]     p         The number of processes involved with this LPF SPMD
 *                          run. Must be larger or equal to zero. May not be
 *                          larger than the value provided by lpf_exec() or
 *                          lpf_hook() via #lpf_spmd_t.
 * \param[in]     lo        The lower process ID participating in these
 *                          collectives. Must be larger or equal to zero. May
 *                          not be larger than \a p. May not be larger than
 *                          \a hi.
 * \param[in]     hi        The upper bound on the process IDs participating in
 *                          these collectives. Must be larger or equal to zero.
 *                          May not be larger than \a p. May not be less than
 *                          \a lo.
 * \param[in]     str       The stride of process IDs. Must be larger or than
 *                          one.
 * \param[in] max_calls     number of collective calls a single LPF process may
 *                          make within a single superstep. Must be larger than
 *                          zero.
 * \param[in] max_elem_size The maximum number of bytes of a single element to
 *                          be reduced through #lpf_reduce or #lpf_allreduce.
 *                          May be equal to zero.
 * \param[in] max_byte_size The maximum number of bytes given as a \a size
 *                          parameter to #lpf_broadcast, #lpf_scatter,
 *                          #lpf_gather, #lpf_allgather, or #lpf_alltoall. May
 *                          be equal to zero.
 * \param[out] coll         The collective struct needed to perform collective
 *                          operations using this library. After a successful
 *                          call to this function, this return #lpf_coll_t
 *                          value may be used immediately.
 *
 * <em>This implementation will register one memory slot on every call to this
 *     function.</em>
 * 
 * \return LPF_ERR_OUT_OF_MEMORY When the requested buffers would cause the
 *                               system to go out of memory. After returning
 *                               with this error code, it shall be as though
 *                               the call to this function had not occurred.
 *
 * \return LPF_SUCCESS  When the function executed successfully.
 */
extern _LPFLIB_API
lpf_err_t lpf_collectives_init_strided(
	lpf_t ctx,
	lpf_pid_t s,
	lpf_pid_t p,
	lpf_pid_t lo,
	lpf_pid_t hi,
	lpf_pid_t str,
	size_t max_calls,
	size_t max_elem_size,
	size_t max_byte_size,
	lpf_coll_t * coll
);

/**
 * Destroys a #lpf_coll_t object created via a call to lpf_collectives_init()
 * or lpf_collectives_init_strided().
 * 
 * This function may only be called on a successfully initialised parameter
 * \a coll. The #lpf_coll_t instance shall then become invalid immediately.
 *
 * This is a collective call conform the descriptions of #lpf_coll_t,
 * #lpf_collectives_init, and #lpf_collectives_init_strided.
 *
 * \param[in] coll The collectives system to invalidate.
 */
extern _LPFLIB_API
lpf_err_t lpf_collectives_destroy( lpf_coll_t coll );

/**
 * Schedules a broadcast of a vector of \a size bytes. The broadcast shall be
 * complete by the end of a next call to lpf_sync(). This is a collective call,
 * meaning that if one LPF process calls this function, all LPF processes in the
 * same SPMD section must make a matching call to this function.
 *
 * The root process has \a size bytes to transmit, as uniquely identified by
 * \a data and \a size.
 *
 * For all \f$ i \in \{ 0, 1, \ldots, \mathit{size} - 1 \} \f$, after a next
 * call to lpf_sync() all processes with non- \a root ID will locally have that
 *     \f$ \mathit{dst}_{ i } \f$ equals
 *     \f$ \mathit{src}_{ i } \f$ at PID \a root.
 *
 * \note No more than \f$\max( P+1, 2P - 3)\f$ messages have to be reserved in
 *       advance with lpf_resize_message_queue()
 *
 * \note No memory areas will be written to on the process with PID \a root.
 * 
 * \note No supplied memory areas will be read from on processes with PID not
 *       equal to \a root. Internally, the collective state memory slot may be 
 *       read from on all processes if a two-stage implementation is used.
 * 
 * \param[in,out] coll   The LPF collectives state.
 *                       <em>This argument must match across processes in the
 *                           same collective call to this function and must 
 *                           provide the same sized memory area of size bytes.</em>
 * \param[in]     src    At PID \a root, the memory slot of the source memory
 *                       area to read from. This argument is only used read from
 *                       by the process with PID equal to \a root; there, the 
 *                       memory slot must correspond to a valid memory area of
 *                       size at least \a size bytes. At processes with PID not
 *                       equal to \a root, the memory area corresponding to this
 *                       slot will not be touched and may thus have any size,
 *                       including zero.
 *                       <b>The memory slot must be global</b>;
 *                       <em>this argument must match across processes in the
 *                           same collective call to this function.</em>
 * \param[in]     dst    At PID not equal to \a root, the destination memory
 *                       area. The memory slot must correspond to a valid memory
 *                       area of size at least \a size bytes. At the process
 *                       with PID \a root the memory area corresponding to this
 *                       slot will not be touched and may be of any size,
 *                       including zero. The memory slot may be local or global.
 *                       Arguments do not have to match across processes in the
 *                       same collective call to this function.
 * \param[in]     size   The number of bytes to broadcast.
 *                       <em>This argument must match across processes in the
 *                           same collective call to this function.</em>
 * \param[in]     root   The PID of the root process of this operation.
 *                       <em>This argument must match across processes in the
 *                           same collective call to this function.</em>
 *
 * \note It is legal to have that \a src equals \a dst.
 *
 * \note At PID \a root, the \a dst memory slot may equal #LPF_INVALID_MEMSLOT.
 * 
 * \parblock
 * \par Performance guarantees: serial
 * -# Problem size N: \f$ size \f$
 * -# local work: \f$ 0 \f$ ;
 * -# transferred bytes: \f$ NP \f$ ;
 * -# BSP cost: \f$ NPg + l \f$;
 * \endparblock
 *
 * \parblock
 * \par Performance guarantees: two phase
 * -# Problem size N: \f$ size \f$
 * -# local work: \f$ 0 \f$ ;
 * -# transferred bytes: \f$ 2N \f$ ;
 * -# BSP cost: \f$ 2(Ng + l) \f$;
 * \endparblock
 *
 * \parblock
 * \par Performance guarantees: two level tree
 * -# Problem size N: \f$ size \f$
 * -# local work: \f$ 0 \f$ ;
 * -# transferred bytes: \f$ 2\sqrt{P}N \f$ ;
 * -# BSP cost: \f$ 2(\sqrt{P}Ng + l) \f$;
 * \endparblock
 *
 */
extern _LPFLIB_API
lpf_err_t lpf_broadcast(
	lpf_coll_t coll,
	lpf_memslot_t src,
	lpf_memslot_t dst,
	size_t size,
	lpf_pid_t root
);

/**
 * Schedules a gather of a vector of \a size bytes. The gather shall be complete
 * by the end of a next call to lpf_sync(). This is a collective operation,
 * meaning that if one LPF process calls this function, all LPF processes in the
 * same SPMD section must make a matching call to this function.
 * 
 * The root process will retrieve \a size bytes from each other process. The
 * source memory areas are identified by \a src and \a size. 
 *
 * For all \f$ i \in \{ 0, 1, \ldots, \mathit{size} - 1 \} \f$ and for all
 * \f$ k \in \{ 0, 1, \ldots, p-1 \},\ k \neq \mathit{root} \f$, after a next
 * call to lpf_sync() the process with ID \a root will have that
 *     \f$ \mathit{dst}_{ k \cdot p + i } \f$
 * equals
 *     \f$ \mathit{src}_{ i } \f$
 * at PID \f$ k \f$, with \f$ p \f$ the number of processes registered in 
 * \a coll. The memory area starting from \a dst plus <tt>root * size</tt> with
 * size \a size bytes will not be touched at PID \a root.
 *
 * \note No more than \f$P-1\f$ messages have to be reserved in advance with
 *       lpf_resize_message_queue()
 *
 * \note No memory areas will be written to at process with PIDs not equal to
 * \a root.
 *
 * \note Recommended usage has at PID \a root that the local source data is
 *       already in place at \a dst with offset \a root times \a size bytes.
 *       Otherwise, a manual copy of the corresponding source data at PID
 *       \a root into \a dst is required to make the memory area at \a dst of
 *       size \a size correspond to the globally gathered data.
 * 
 * \param[in,out] coll   The LPF collectives state.
 *                       <em>This argument must match across processes in the
 *                           same collective call to this function.</em>
 * \param[in]     src    The memory slot of the source memory area to read from.
 *                       When PID is not \a root, the corresponding memory area
 *                       to \a src should be valid to read for at least \a size
 *                       bytes. When PID is \a root, the corresponding memory
 *                       area will not be touched and can be of any size,
 *                       including zero. The memory slot can be local or global.
 *                       This argument may differ across processes in the same
 *                       collective call to this function.
 * \param[in]     dst    The memory slot of the destination memory area.
 *                       At process ID \a root, the corresponding memory area
 *                       must must be valid and of size at least
 *                       <tt>p * size</tt> bytes. At other processes, the
 *                       corresponding memory area will not be touched and may
 *                       be of any size. <b>The memory slot must be global</b>;
 *                       <em>this argument must match across processes in the
 *                           same collective call to this function.</em>
 * \param[in]     size   The number of bytes at each source array.
 *                       <em>This argument must match across processes in the
 *                           same collective call to this function.</em>
 * \param[in]     root   Which process is the root of this operation.
 *                       <em>This argument must match across processes in the
 *                           same collective call to this function.</em>
 *
 * \note It is legal to have that \a src equals \a dst.
 *
 * \note At PID \a root, the \a src memory slot may equal #LPF_INVALID_MEMSLOT.
 * 
 * \parblock
 * \par Performance guarantees:
 * -# Problem size N: \f$ P * size \f$
 * -# local work: \f$ 0 \f$ ;
 * -# transferred bytes: \f$ N \f$ ;
 * -# BSP cost: \f$ Ng + l \f$;
 * \endparblock
 *
 */
extern _LPFLIB_API
lpf_err_t lpf_gather(
	lpf_coll_t coll,
	lpf_memslot_t src,
	lpf_memslot_t dst,
	size_t size,
	lpf_pid_t root
);

/**
 * Schedules a scatter of a vector of \a size bytes. The operation shall be
 * complete by the end of a next call to lpf_sync(). This is a collective
 * operation, meaning that if one LPF process calls this function, all LPF
 * processes in the same SPMD section must make a matching call to this
 * function.
 * 
 * The root process will split a source memory area in segments of \a size
 * bytes each, while expecting \f$ p \f$ segments. Each \f$ k \f$th segment is
 * sent to process \f$ k \f$.
 * 
 * The \f$ k \f$-th process, \f$ k \neq \mathit{root} \f$, shall, after the next
 * call to #lpf_sync, have that for all \f$ 0 \leq i < \mathit{size} \f$, 
 *     \f$ \mathit{dest}_i \f$
 * equals
 *     \f$ \mathit{src}_{ k \mathit{size} + i } \f$
 * at PID \a root.
 *
 * \note No more than \f$P-1\f$ messages have to be reserved in advance with
 *       lpf_resize_message_queue()
 *
 * \note No memory shall be written to at the process with PID \a root.
 *
 * \note No memory shall be read from at processes with PID other than \a root.
 *
 * \param[in,out] coll   The LPF collectives state.
 *                       <em>This argument must match across processes in the
 *                           same collective call to this function.</em>
 * \param[in]     src    The memory slot of the source memory area to read from.
 *                       When the PID equals \a root, this must point to a valid
 *                       memory area of size at least \f$ p \mathit{size} \f$
 *                       bytes.
 *                       For all other processes, the corresponding memory area
 *                       will not be touched and may be of any size, including
 *                       zero. <b>The slot must be global;</b>
 *                       <em>this argument must match across processes in the
 *                           same collective call to this function.</em>
 * \param[in]     dst    The memory slot of the destination memory area. At
 *                       processes with PID not equal to \a root, this must
 *                       point to a valid memory area of at least \a size
 *                       bytes. At PID \a root, the memory area is not touched
 *                       and may be of any size, including zero. This argument
 *                       may differ across processes in the same collective call
 *                       to this function.
 * \param[in]     size   The number of bytes that need to be scattered to a
 *                       single process. The total number of bytes scattered,
 *                       i.e., the size of the \a src memory area, equals
 *                       \f$ p \mathit{size} \f$.
 *                       <em>This argument must match across processes in the
 *                           same collective call to this function.</em>
 * \param[in]     root   Which process is the root of this operation.
 *                       <em>This argument must match across processes in the
 *                           same collective call to this function.</em>
 *
 * \note It is legal to have that \a src equals \a dst.
 *
 * \note At PID \a root, the \a dst memory slot may equal #LPF_INVALID_MEMSLOT.
 * 
 * \parblock
 * \par Performance guarantees:
 * -# Problem size N: \f$ size \f$
 * -# local work: \f$ 0 \f$ ;
 * -# transferred bytes: \f$ N \f$ ;
 * -# BSP cost: \f$ Ng + l \f$;
 * \endparblock
 *
 */
extern _LPFLIB_API
lpf_err_t lpf_scatter(
	lpf_coll_t coll,
	lpf_memslot_t src,
	lpf_memslot_t dst,
	size_t size,
	lpf_pid_t root
);

/**
 * Schedules an allgather of a vector of \a size bytes. The operation shall be
 * complete by the end of a next call to lpf_sync(). This is a collective,
 * operation, meaning that if one LPF process calls this function, all LPF
 * processes in the same SPMD section must make a matching call to this
 * function.
 * 
 * All processes locally have two memory areas; one of <tt>size</tt> bytes and
 * another of <tt>p * size</tt> bytes. After the next call to #lpf_sync, each
 * process will have at the latter memory area a concatenation of all of the
 * former memory areas from all processes. More formally:
 * 
 * At the end of the next call to #lpf_sync, each process with its PID
 *     \f$ s \in \{ 0, 1, \ldots, p-1 \} \f$
 * has \f$ \forall i \in \{ 0, 1, \ldots, \mathit{size}-1 \} \f$
 * and \f$ \forall k \in \{ 0, 1, \ldots, p-1 \},\ k \neq s \f$ that
 *     \f$ \mathit{dst}_{ k \cdot \mathit{size} + i } \f$
 * local to PID \f$ s \f$ equals
 *     \f$ \mathit{src}_{ i } \f$
 * local to PID \f$ k \f$.
 * 
 * \note No more than \f$2*P\f$ messages have to be reserved in advance with
 *       lpf_resize_message_queue()
 *
 * \note There will be no communication outgoing from a process that is incident
 *       to that same process, unless exclude_myself is false (see below).
 *
 * The induced communication pattern as defined above must never cause read and
 * writes to occur at the same memory location, or undefined behaviour will
 * occur.
 * 
 * \param[in,out] coll The LPF collectives state.
 *                     <em>This argument must match across processes in the
 *                         same collective call to this function.</em>
 * \param[in]     src  The memory slot of the source memory area. <b>This can
 *                     be a local or global slot.</b> The memory area must be 
 *                     at least \a size bytes large.
 *                     This argument may differ across processes in the same
 *                     collective call to this function. This argument must not
 *                     equal \a dst.
 * \param[in]     dst  The memory slot of the destination memory area. This must
 *                     be a global slot. On all processes, this must correspond
 *                     to a valid memory area of size at least
 *                     <tt>p * size</tt>. <b>This must be a global slot</b>;
 *                     <em>this argument must match across processes in the
 *                         same collective call to this function.</em>
 * \param[in]     size The number of bytes in a single source memory area.
 *                     <em>This argument must match across processes in the
 *                         same collective call to this function.</em>
 * \param[in] exclude_myself Skip myself in the collective communication.
 *
 * \note The memory area corresponding to \a src may overlap with the memory
 *       pointed to \a dst, within any single process. This happens and is
 *       valid exactly when the memory area pointed to by \a src points to
 *       that of \a dst with an offset of exactly <tt> s * size </tt>, with
 *       <tt>s</tt> that process' PID, and \a src was registered with length
 *       exactly <tt>size</tt> bytes.
 *
 * \warning An implementation does not have to check for erroneously
 *          overlapping calls-- any such call may be mitigated by an
 *          implementation, but
 *          <em>will in general lead to undefined behaviour.</em>
 * 
 * \parblock
 * \par Performance guarantees:
 * -# Problem size N: \f$ P * size \f$
 * -# local work: \f$ 0 \f$ ;
 * -# transferred bytes: \f$ N \f$ ;
 * -# BSP cost: \f$ Ng + l \f$;
 * \endparblock
 *
 */
extern _LPFLIB_API
lpf_err_t lpf_allgather(
	lpf_coll_t coll,
	lpf_memslot_t src,
	lpf_memslot_t dst,
	size_t size,
	bool exclude_myself
);

/** 
 * Schedules an all-to-all of a vector of \a size bytes. The operation shall be
 * complete by the end of a next call to lpf_sync(). This is a collective
 * operation, meaning that if one LPF process calls this function, all LPF
 * processes in the same SPMD section must make a matching call to this
 * function.
 *
 * All process locally have \f$ p \f$ elements of \f$ \mathit{size} \f$ bytes.
 * The elements will be transposed amongst all participating processes.
 *
 * At the end of this operation, each process with its unique ID
 * \f$ s \in \{ 0, 1, \ldots, p-1 \} \f$ has
 *     \f$ \forall i \in \{ 0, 1, \ldots, \mathit{size} - 1 \} \f$
 * and
 *     \f$ \forall k \in \{ 0, 1, \ldots, p - 1 \},\ k \neq s \f$
 * that
 *     \f$ \mathit{dst}_{ k\mathit{size} + i } \f$
 * local to PID \f$ s \f$ equals
 *     \f$ \mathit{src}_{ s\mathit{size} + i } \f$
 * local to PID \f$ k \f$.
 * 
 * It is illegal to have \a src equal to \a dst.
 *
 * \note The \a src memory area shall never be overwritten.
 *
 * \note No more than \f$2*P-2\f$ messages have to be reserved in advance with
 *       lpf_resize_message_queue()
 *
 * \note A process shall never local copy data from \a src to \a dst. To make
 *       \a dst a full transpose of its \a src row at all processes, the
 *       diagonal has to be copied manually.
 *
 * <em>All arguments to this function must match across all processes in the
 *     collective call to this function.</em>
 * 
 * \param[in,out] coll  The LPF collectives state.
 * \param[in]     src   The memory slot of the source memory area. This must
 *                      correspond to a valid  memory area of size at least
 *                      \f$ p \mathit{size} \f$. This memory area must not
 *                      overlap with that of \a dst. <b>This must be a global
 *                      memory slot</b>.
 * \param[in]     dst   The memory slot of the destination memory area to
 *                      write to. On all processes, this must point to a valid
 *                      memory area of size at least \f$ p \mathit{size} \f$.
 *                      This parameter must not overlap with that of \a src.
 *                      <b>This must be a global memory slot</b>.
 * \param[in]     size  The number of bytes that each process sends to another.
 *                      In total, each process sends \f$ (p-1)\mathit{size} \f$
 *                      bytes, and receives the same amount.
 *
 * \warning An implementation does not have to check for use of overlapping
 *          memory areas, although it may use \a src_slot and \a dst_slot to do
 *          so. The user must make sure to never supply aliased memory regions.
 *          If \a src_slot \em does equal \a dst_slot, an implementation thus
 *          \em may have a mechanism to mitigate that error, but <em>in general
 *          this will lead to undefined behaviour.</em>
 *
 * \parblock
 * \par Performance guarantees:
 * -# Problem size N: \f$ P * size \f$
 * -# local work: \f$ 0 \f$ ;
 * -# transferred bytes: \f$ N \f$ ;
 * -# BSP cost: \f$ Ng + l \f$;
 * \endparblock
 *
 */
extern _LPFLIB_API
lpf_err_t lpf_alltoall(
	lpf_coll_t coll,
	lpf_memslot_t src,
	lpf_memslot_t dst,
	size_t size
);

/**
 * Schedules a reduce operation of one array per process. The reduce shall be
 * completed by the end of a next call to lpf_sync(). This is a collective
 * operation, meaning that if one LPF process calls this function, all LPF
 * processes in the same SPMD section must make a matching call to this
 * function.
 * 
 * At the end of the next #lpf_sync, the memory area \a element points to shall
 * equal the reduced value of all the element memory area passed at function
 * entry. This output value shall only be set at the \a root process.
 * The reduction operator is user-defined through a #lpf_reducer_t. Even if the
 * same reducer function is used, this may result in different pointers being
 * passed across the various processes involved in the collective call; hence
 * the \a reducer argument cannot be enforced to be the same everywhere.
 *
 * \note No more than \f$P-1\f$ messages have to be reserved in advance with
 *       lpf_resize_message_queue()
 * 
 * \note Logically, \a reducer should point to the same function or
 *       undeterministic behaviour may result. Only advanced programmers and
 *       applications will be able to exploit this meaningfully.
 * 
 * \param[in,out] coll    The LPF collectives state.
 *                        <em>This argument must match across processes in the
 *                            same collective call to this function.</em>
 * \param[in,out] element At PID \a root, a pointer to a memory area of at least
 *                        \a size bytes. At function entry, the memory area will
 *                        contain the element to be reduced. After a next call
 *                        to #lpf_sync, the value of the memory area at PID
 *                        \a root will equal the globally reduced value.
 *                        At PID not equal to \a root, this memory area at entry
 *                        will not point to the to be reduced value. At exit the
 *                        memory area will not have changed.
 * \param[in] element_slot The #lpf_memslot_t corresponding to \a element. The
 *                         memory slot must be global, and must have registered
 *                         \a size bytes starting from \a element.
 * \param[in] size        The size of a single element of the type to be
 *                        reduced, in bytes.
 *                        <em>This argument must match across processes in the
 *                            same collective call to this function.</em>
 * \param[in] reducer     A function that defines the reduction operator.
 *                        This argument may differ across processes in the same
 *                        collective call to this function.
 * \param[in] root        The process ID of the root process in this collective.
 *                        <em>This argument must match across processes in the
 *                            same collective call to this function.</em>
 *
 * \parblock
 * \par Performance guarantees: allgather (N < P*P)
 * -# Problem size N: \f$ P * size \f$
 * -# local work: \f$ P*reducer \f$ ;
 * -# transferred bytes: \f$ N \f$ ;
 * -# BSP cost: \f$ Ng + N*reducer + l \f$;
 * \endparblock
 *
 * \parblock
 * \par Performance guarantees: transpose, reduce and allgather (N >= P*P)
 * -# Problem size N: \f$ P * size \f$
 * -# local work: \f$ (N/P)*reducer \f$ ;
 * -# transferred bytes: \f$ 2(N/P) \f$ ;
 * -# BSP cost: \f$ 2(N/P)g + (N/P)*reducer + 2l \f$;
 * \endparblock
 *
 */
extern _LPFLIB_API
lpf_err_t lpf_reduce(
	lpf_coll_t coll,
	void * restrict element,
	lpf_memslot_t element_slot,
	size_t size,
	lpf_reducer_t reducer,
	lpf_pid_t root
);

/**
 * Schedules an allreduce operation of a single object per process. The
 * allreduce shall be complete by the end of a next call to #lpf_sync. This is
 * a collective operation, meaning that if one LPF process calls this function,
 * all LPF processes in the same SPMD section must make a matching call to this
 * function.
 * 
 * At the end of the next #lpf_sync, the memory area \a element points to shall
 * equal the reduced value of all elements at all processes.
 * The reduction operator is user-defined through a #lpf_reducer_t. Even if the
 * same reducer function is used, this may result in different pointers being
 * passed across the various processes involved in the collective call; hence
 * the \a reducer argument cannot be enforced to be the same everywhere.
 * 
 * \note No more than \f$2*P-2\f$ messages have to be reserved in advance with
 *       lpf_resize_message_queue()
 *
 * \note Logically, \a reducer should point to the same function or
 *       undeterministic behaviour may result. Only advanced programmers and
 *       applications will be able to exploit this meaningfully.
 * 
 * \param[in,out] coll     The LPF collectives state.
 *                         <em>This argument must match across processes in the
 *                             same collective call to this function.</em>
 * \param[in,out] element  A pointer to a memory area of at least \a size bytes.
 *                         At function entry, this equals the local to be
 *                         reduced value.
 *                         After a next call to #lpf_sync, the value of the
 *                         memory area will equal the globally reduced value.
 * \param[in] element_slot The #lpf_memslot_t corresponding to \a element. The
 *                         memory slot must be global, and must have registered
 *                         \a size bytes starting from \a element.
 * \param[in] size         The size of a single element of the type to be
 *                         reduced, in bytes.
 *                         <em>This argument must match across processes in the
 *                             same collective call to this function.</em>
 * \param[in] reducer      A function that defines the reduction operator.
 *                         This argument may differ across processes in the same
 *                         collective call to this function.
 *
 * \parblock
 * \par Performance guarantees: allgather (N < P*P)
 * -# Problem size N: \f$ P * size \f$
 * -# local work: \f$ P*reducer \f$ ;
 * -# transferred bytes: \f$ N \f$ ;
 * -# BSP cost: \f$ Ng + N*reducer + l \f$;
 * \endparblock
 *
 * \parblock
 * \par Performance guarantees: transpose, reduce and allgather (N >= P*P)
 * -# Problem size N: \f$ P * size \f$
 * -# local work: \f$ (N/P)*reducer \f$ ;
 * -# transferred bytes: \f$ 2(N/P) \f$ ;
 * -# BSP cost: \f$ 2(N/P)g + (N/P)*reducer + 2l \f$;
 * \endparblock
 *
 */
extern _LPFLIB_API
lpf_err_t lpf_allreduce(
	lpf_coll_t coll,
	void * restrict element,
	lpf_memslot_t element_slot,
	size_t size,
	lpf_reducer_t reducer
);

/**
 * Combines an array at all non-root processes into that of the \a root
 * process.
 * 
 * The operation is guaranteed to be complete after a next call to #lpf_sync.
 *
 * On input, all processes must supply a valid data \a array. On output, the
 * \a root process will have its output \a array equal to 
 * \f$ array^{\mathit{root}} = \oplus_{k=0}^{p-1} array^{(k)}, \f$
 * where \f$ \oplus \f$ is prescribed by \a combiner. The order in which the
 * operator \f$ \oplus \f$ is applied is undefined. The operator \f$ \oplus \f$
 * may furthermore be applied in stages, when applied to a single set of input
 * and output arrays. See #lpf_combiner_t for more details.
 *
 * <em>All parameters must match across all processes involved with the same
 *     collective call to this function.</em>
 *
 * This implementation synchronises once before exiting.
 *
 * \note No more than \f$2*P\f$ messages have to be reserved in advance with
 *       lpf_resize_message_queue()
 * 
 * \param[in,out]  coll The LPF collectives state.
 * \param[in,out] array The array to be combined. The array must point to a
 *                      valid memory area of size
 *                      \f$ \mathit{num}\mathit{size} \f$. The \a num elements
 *                      in this array must be initialised and will be taken as
 *                      input for the \a combiner. After a call to this
 *                      function, this array's contents will be undefined. On
 *                      the \a root process, this array's contents will be the
 *                      combination of all arrays, as prescribed by the 
 *                      \a combiner.
 * \param[in]      slot The memory slot corresponding to \a array. <b>This
 *                      must be a globally registered slot.</b>
 * \param[in]       num The number of elements in the \a array.
 * \param[in]      size The size, in bytes, of a single element of the \a array.
 * \param[in]  combiner A function which may combine one or more elements of the
 *                      appropriate array types. The combining happens element-
 *                      by-element.
 * \param[in]      root Which process is the root of this collective operation.
 *
 * \returns #LPF_SUCCESS When the collective communication request was
 *                           recorded successfully.
 *
 * \parblock
 * \par Performance guarantees: allgather (N < P*P)
 * -# Problem size N: \f$ P * num * size \f$
 * -# local work: \f$ N*Operator \f$ ;
 * -# transferred bytes: \f$ N \f$ ;
 * -# BSP cost: \f$ Ng + N*Operator + l \f$;
 * \endparblock
 *
 * \parblock
 * \par Performance guarantees: transpose, reduce and allgather (N >= P*P)
 * -# Problem size N: \f$ P * num * size \f$
 * -# local work: \f$ (N/P)*Operator \f$ ;
 * -# transferred bytes: \f$ 2(N/P) \f$ ;
 * -# BSP cost: \f$ 2(N/P)g + (N/P)*Operator + 2l \f$;
 * \endparblock
 *
 * \parblock
 * \par Performance guarantees: two level tree
 * -# Problem size N: \f$ P * num * size \f$
 * -# local work: \f$ 2(N/\sqrt{P})*Operator \f$ ;
 * -# transferred bytes: \f$ 2(N/\sqrt{P}) \f$ ;
 * -# BSP cost: \f$ 2(N/\sqrt{P})g + (N/\sqrt{P})*Operator + 2l \f$;
 * \endparblock
 *
 */
extern _LPFLIB_API
lpf_err_t lpf_combine(
	lpf_coll_t coll,
	void * restrict array,
	lpf_memslot_t slot,
	size_t num,
	size_t size,
	lpf_combiner_t combiner,
	lpf_pid_t root
);

/**
 * Combines an array at all processes into one array that is broadcasted over
 * all processes.
 * 
 * The operation is guaranteed to be complete after a next call to #lpf_sync.
 *
 * On input, all processes must supply a valid data \a array. On output, the
 * all processes will have their output \a array equal to 
 *    \f$ \oplus_{k=0}^{p-1} array^{(k)}, \f$
 * where \f$ \oplus \f$ is prescribed by \a combiner. The order in which the
 * operator \f$ \oplus \f$ is applied is undefined. The operator \f$ \oplus \f$
 * may furthermore be applied in stages, when applied to a single set of input
 * and output arrays. See #lpf_combiner_t for more details.
 *
 * <em>All parameters must match across all processes involved with the same
 *     collective call to this function.</em>
 *
 * \note No more than \f$2*P\f$ messages have to be reserved in advance with
 *       lpf_resize_message_queue()
 *
 * \param[in,out]  coll The LPF collectives state.
 * \param[in]      size The size, in bytes, of a single element of the \a array.
 * \param[in]       num The number of elements in the \a array.
 * \param[in]  combiner A function which may combine one or more elements of the
 *                      appropriate array types. The combining happens element-
 *                      by-element.
 * \param[in,out] array The array to be combined. The array must point to a
 *                      valid memory area of size
 *                      \f$ \mathit{num}\mathit{size} \f$. The \a num elements
 *                      in this array must be initialised and will be taken as
 *                      input for the \a combiner. After a call to this
 *                      function, this array's contents will be undefined. On
 *                      the \a root process, this array's contents will be the
 *                      combination of all arrays, as prescribed by the 
 *                      \a combiner.
 * \param[in]      slot The memory slot corresponding to \a array. <b>This
 *                      must be a globally registered slot.</b>
 *
 * \returns #LPF_SUCCESS When the collective communication request was
 *                           recorded successfully.
 *
 * \parblock
 * \par Performance guarantees: allgather (N < P*P)
 * -# Problem size N: \f$ P * num * size \f$
 * -# local work: \f$ N*Operator \f$ ;
 * -# transferred bytes: \f$ N \f$ ;
 * -# BSP cost: \f$ Ng + N*Operator + l \f$;
 * \endparblock
 *
 * \parblock
 * \par Performance guarantees: transpose, reduce and allgather (N >= P*P)
 * -# Problem size N: \f$ P * num * size \f$
 * -# local work: \f$ (N/P)*Operator \f$ ;
 * -# transferred bytes: \f$ 2(N/P) \f$ ;
 * -# BSP cost: \f$ 2(N/P)g + (N/P)*Operator + 2l \f$;
 * \endparblock
 *
 * \parblock
 * \par Performance guarantees: two level tree
 * -# Problem size N: \f$ P * num * size \f$
 * -# local work: \f$ 2(N/\sqrt{P})*Operator \f$ ;
 * -# transferred bytes: \f$ 2(N/\sqrt{P}) \f$ ;
 * -# BSP cost: \f$ 2(N/\sqrt{P})g + (N/\sqrt{P})*Operator + 2l \f$;
 * \endparblock
 *
 */
extern _LPFLIB_API
lpf_err_t lpf_allcombine(
	lpf_coll_t coll,
	void * restrict array,
	lpf_memslot_t slot,
	size_t num,
	size_t size,
	lpf_combiner_t combiner
);

/** 
 * @} 
 *
 * @}
 * 
 */

#ifdef __cplusplus
} //end ``extern "C"''
#undef restrict
#endif

#endif //end ``_H_LPF_COLLECTIVES''


