
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

#ifndef LPFLIB_NOC_H
#define LPFLIB_NOC_H

// import size_t data type for the implementation
#ifndef DOXYGEN

#ifdef __cplusplus
#include <cstddef>
#else
#include <stddef.h>
#endif

#ifndef _LPF_NOC_STANDALONE
 #include <lpf/core.h>
#else
 #include <lpf/noc-standalone.h>
 #define _LPFLIB_API
#endif

#endif // DOXYGEN


#ifdef __cplusplus
extern "C" {
#endif

/** \addtogroup LPF_EXTENSIONS LPF API extensions
 *
 * @{
 *
 * \defgroup LPF_NOC Extensions to LPF where it need not maintain consistency.
 *
 * This extension specifies facilities for (de-)registering memory slots,
 * registering RDMA requests, and fencing RDMA requests. These extensions are,
 * as far as possible, fully compatible with the core LPF definitions. These
 * include LPF contexts (#lpf_t), processor count types (#lpf_pid_t), memory
 * slot types (#lpf_memslot_t), and message attributes (#lpf_msg_attr_t).
 *
 * In this extension, LPF does not maintain consistency amongst processes that
 * (de-)register memory slots while RDMA communication may occur. Maintaining
 * the required consistency instead becomes the purview of the user. This
 * extension specificies exactly what consistency properties the user must
 * guarantee.
 *
 * \warning If LPF is considered a tool for the so-called <em>hero
 *          programmer</em>, then please note that this variant is even harder
 *          to program with.
 *
 * \note At present, no debug layer exists for this extension. It is unclear if
 *       such a debug layer is even possible (precisely because LPF in this
 *       extension does not maintain consistency, there is no way a debug layer
 *       could enforce it).
 *
 * @{
 */


/**
 * The version of this no-conflict LPF specification. All implementations shall
 * define this macro. The format is YYYYNN, where YYYY is the year the
 * specification was released, and NN the number of the specifications released
 * before this one in the same year.
 */
#define _LPF_NOC_VERSION 202400L

/**
 * Resizes the memory register for non-coherent RDMA.
 *
 * After a successful call to this function, the local process has enough
 * resources to register \a max_regs memory regions in a non-coherent way.
 *
 * Each registration via lpf_noc_register() counts as one. Such registrations
 * remain taking up capacity in the register until they are released via a call
 * to lpf_noc_deregister(), which lowers the count of used memory registerations
 * by one.
 *
 * There are no runtime out-of-bounds checks prescribed for lpf_noc_register()--
 * this would also be too costly as error checking would require communication.
 *
 * If memory allocation were successful, the return value is #LPF_SUCCESS and
 * the local process may assume the new buffer size \a max_regs.
 *
 * In the case of insufficient local memory the return value will be
 * #LPF_ERR_OUT_OF_MEMORY. In that case, it is as if the call never happened and
 * the user may retry the call locally after freeing up unused resources. Should
 * retrying not lead to a successful call, the programmer may opt to broadcast
 * the error (using existing slots) or to give up by returning from the spmd
 * section.
 *
 * \note The current maximum cannot be retrieved from the runtime. Instead, the
 *       programmer must track this information herself. To provide
 *       encapsulation, see lpf_rehook().
 *
 * \note When the given memory register capacity is smaller than the current
 *       capacity, the runtime is allowed but not required to release the
 *       allocated memory. Such a call shall always be successful and return
 *       #LPF_SUCCESS.
 *
 * \note This means that an implementation that allows shrinking the given
 *       capacity must also ensure the old buffer remains intact in case there
 *       is not enough memory to allocate a smaller one.
 *
 * \note The last invocation of lpf_noc_resize_memory_register() determines the
 *       maximum number of memory registrations using lpf_noc_register() that
 *       can be maintained concurrently.
 *
 * \par Thread safety
 * This function is safe to be called from different LPF processes only. Any
 * further thread safety may be guaranteed by the implementation, but is not
 * specified. Similar conditions hold for all LPF primitives that take an
 * argument of type #lpf_t; see #lpf_t for more information.
 *
 * \param[in,out]   ctx The runtime state as provided by lpf_exec().
 * \param[in]  max_regs The requested maximum number of memory regions that can
 *                      be registered. This value must be the same on all
 *                      processes.
 *
 * \returns #LPF_SUCCESS
 *            When this process successfully acquires the resources.
 *
 * \returns #LPF_ERR_OUT_OF_MEMORY
 *            When there was not enough memory left on the heap. In this case
 *            the effect is the same as when this call did not occur at all.
 *
 * \par BSP costs
 * None
 *
 * See also \ref BSPCOSTS.
 *
 * \par Runtime costs
 * \f$ \Theta( \mathit{max\_regs} ) \f$.
 */
extern _LPFLIB_API
lpf_err_t lpf_noc_resize_memory_register( lpf_t ctx, size_t max_regs );

/**
 * Registers a local memory area, preparing its use for intra-process
 * communication.
 *
 * The registration process is necessary to enable Remote Direct Memory Access
 * (RDMA) primitives, such as lpf_get() and lpf_put().
 *
 * This is \em not a collective function. For #lpf_get and #lpf_put, the memory
 * slot returned by this function is equivalent to a memory slot returned by
 * #lpf_register_local; the \a memslot returned by a successful call to this
 * function (hence) is immediately valid. A successful call (hence) immediately
 * consumes one memory slot capacity; see also #lpf_resize_memory_register on
 * how to ensure sufficient capacity.
 *
 * Different from a memory slot returned by #lpf_register_local, a memory slot
 * returned by a successful call to this function may serve as either a local
 * or remote memory slot for #lpf_noc_put and #lpf_noc_get.
 *
 * Use of the returned memory slot to indicate a remote memory area may only
 * occur by copying the returned memory slot to another LPF process. This may
 * be done using the standard #lpf_put and #lpf_get methods or by using
 * auxiliary communication mechanisms. The memory slot thus communicated only
 * refers to a valid memory area on the process it originated from; any other
 * use leads to undefined behaviour.
 *
 * \note Note that the ability to copy memory slots to act as identifiers of
 *       remote areas exploits the LPF core specification that instances of
 *       the #lpf_memslot_t type are, indeed, byte-copyable.
 *
 * A memory slot returned by a successful call to this function may be
 * destroyed via a call to the standard #lpf_deregister. The deregistration
 * takes effect immediately. No communication using the deregistered slot
 * should occur during that superstep, or otherwise undefined behaviour occurs.
 *
 * Only the process that created the returned memory slot can destroy it; other
 * LPF processes than the one which created it that attempt to destroy the
 * returned memory slot, invoke undefined behaviour.
 *
 * Other than the above specified differences, the arguments to this function
 * are the same as for #lpf_register_local:
 *
 * \param[in,out] ctx     The runtime state as provided by lpf_exec().
 * \param[in]     pointer The pointer to the memory area to register.
 * \param[in]     size    The size of the memory area to register in bytes.
 * \param[out]    memslot Where to store the memory slot identifier.
 *
 * \note Registering a slot with zero \a size is valid. The resulting memory
 *       slot cannot be written to nor read from by remote LPF processes.
 *
 * \note In particular, passing \c NULL as \a pointer and \c 0 for \a size is
 *       valid.
 *
 * \returns #LPF_SUCCESS
 *            Successfully registered the memory region and successfully
 *            assigned a memory slot identifier.
 *
 * \note One registration consumes one memory slot from the pool of locally
 *       available memory slots, which must have been preallocated by
 *       lpf_resize_memory_register() or recycled by lpf_deregister(). Always
 *       use lpf_resize_memory_register() at the start of the SPMD function
 *       that is executed by lpf_exec(), since lpf_exec() itself does not
 *       preallocate slots.
 *
 * \note It is illegal to request more memory slots than have previously been
 *       registered with lpf_resize_memory_register(). There is no runtime
 *       check for this error, because a safe way out cannot be guaranteed
 *       without significant parallel error checking overhead.
 *
 * \par Thread safety
 * This function is safe to be called from different LPF processes only. Any
 * further thread safety may be guaranteed by the implementation, but is not
 * specified. Similar conditions hold for all LPF primitives that take an
 * argument of type #lpf_t; see #lpf_t for more information.
 *
 * \par BSP costs
 *
 * None.
 *
 * \par Runtime costs
 *
 * \f$ \mathcal{O}( \texttt{size} ) \f$.
 *
 * \note This asymptotic bound may be attained for implementations that require
 *       linear-time processing on the registered memory area, such as to effect
 *       memory pinning. If this is not required, a good implementation will
 *       require only \f$ \Theta(1) \f$ time.
 */
extern _LPFLIB_API
lpf_err_t lpf_noc_register(
    lpf_t ctx,
    void * pointer,
    size_t size,
    lpf_memslot_t * memslot
);

/**
 * Deregisters a memory area previously registered using lpf_noc_register().
 *
 * After a successful deregistration, the slot is returned to the pool of free
 * memory slots. The total number of memory slots may be set via a call to
 * lpf_noc_resize_memory_register().
 *
 * Deregistration takes effect immediately. A call to this function is not
 * collective, and the other of deregistration does not need to match the order
 * of registration. Any local or remote communication using the given \a memslot
 * in the current superstep invokes undefined behaviour.
 *
 * \par Thread safety
 * This function is safe to be called from different LPF processes only. Any
 * further thread safety may be guaranteed by the implementation, but is not
 * specified. Similar conditions hold for all LPF primitives that take an
 * argument of type #lpf_t; see #lpf_t for more information.
 *
 * \param[in,out] ctx The runtime state as provided by lpf_exec().
 * \param[in] memslot The memory slot identifier to de-register.
 *
 * \returns #LPF_SUCCESS
 *            Successfully deregistered the memory region.
 *
 * \par BSP costs
 * None.
 *
 * \par Runtime costs
 * \f$ \mathcal{O}(n) \f$, where \f$ n \f$ is the size of the memory region
 * corresponding to \a memslot.
 */
extern _LPFLIB_API
lpf_err_t lpf_deregister(
    lpf_t ctx,
    lpf_memslot_t memslot
);

/**
 * Copies contents of local memory into the memory of remote processes.
 *
 * This operation is guaranteed to be completed after a call to the next
 * lpf_sync() exits.
 *
 * Until that time it occupies one entry in the operations queue.
 *
 * Concurrent reads or writes from or to the same memory area are
 * allowed in the same way they are for the core primitive #lpf_put.
 *
 * This primitive differs from #lpf_put in that the \a dst_slot may be the
 * result of a successful call to #lpf_noc_register, while \a src_slot \em must
 * be the results of such a successful call. In both cases, the slot need
 * \em not have been registered before the last call to #lpf_sync.
 *
 * \par Thread safety
 * This function is safe to be called from different LPF processes only. Any
 * further thread safety may be guaranteed by the implementation, but is not
 * specified. Similar conditions hold for all LPF primitives that take an
 * argument of type #lpf_t; see #lpf_t for more information.
 *
 * \param[in,out] ctx    The runtime state as provided by lpf_exec()
 * \param[in] src_slot   The memory slot of the local source memory area
 *                       registered using lpf_register_local(),
 *                       lpf_register_global(), or lpf_noc_register()
 * \param[in] src_offset The offset of reading out the source memory area,
 *                       w.r.t. the base location of the registered area
 *                       expressed in bytes.
 * \param[in] dst_pid    The process ID of the destination process.
 * \param[in] dst_slot   The memory slot of the destination memory area at
 *                       \a pid, registered using lpf_register_global() or
 *                       lpf_noc_register().
 * \param[in] dst_offset The offset of writing to the destination memory area
 *                       w.r.t. the base location of the registered area
 *                       expressed in bytes.
 * \param[in] size       The number of bytes to copy from the source memory area
 *                       to the destination memory area.
 * \param[in] attr
 *            \parblock
 *            In case an \a attr not equal to #LPF_MSG_DEFAULT is provided, the
 *            the message created by this function may have modified semantics
 *            that may be used to extend this API. Examples include:
 *
 *              -# delaying the superstep deadline of delivery, and/or
 *              -# DRMA with message combining semantics.
 *
 *            These attributes are stored after a call to this function has
 *            completed and may be modified immediately after without affecting
 *            any messages already scheduled.
 *            \endparblock
 *
 * \note See #lpf_put for notes regarding #lpf_msg_attr_t.
 *
 * \returns #LPF_SUCCESS
 *            When the communication request was recorded successfully.
 *
 * \par BSP costs
 * This function will increase
 *     \f$ t_{c}^{(s)} \f$
 * and
 *     \f$ r_{c}^{(\mathit{pid})} \f$
 * by \a size, where c is the current superstep number and s is this process ID
 * (as provided by #lpf_exec)). See \ref BSPCOSTS on how this affects real-time
 * communication costs.
 *
 * \par Runtime costs
 * See \ref BSPCOSTS.
 */
extern _LPFLIB_API
lpf_err_t lpf_noc_put(
    lpf_t ctx,
    lpf_memslot_t src_slot,
    size_t src_offset,
    lpf_pid_t dst_pid,
    lpf_memslot_t dst_slot,
    size_t dst_offset,
    size_t size,
    lpf_msg_attr_t attr
);

/**
 * Copies contents from remote memory to local memory.
 *
 * This operation completes after one call to lpf_sync().
 *
 * Until that time it occupies one entry in the operations queue.
 *
 * Concurrent reads or writes from or to the same memory area are allowed in the
 * same way it is for #lpf_get.
 *
 * This primitive differs from #lpf_get in that the \a src_slot may be the
 * result of a successful call to #lpf_noc_register, while \a dst_slot \em must
 * be the results of such a successful call. In both cases, the slot need
 * \em not have been registered before the last call to #lpf_sync.
 *
 * \par Thread safety
 * This function is safe to be called from different LPF processes only. Any
 * further thread safety may be guaranteed by the implementation, but is not
 * specified. Similar conditions hold for all LPF primitives that take an
 * argument of type #lpf_t; see #lpf_t for more information.
 *
 * \param[in,out] ctx    The runtime state as provided by lpf_exec().
 * \param[in] src_pid    The process ID of the source process.
 * \param[in] src_slot   The memory slot of the source memory area at \a pid, as
 *                       globally registered with lpf_register_global() or
 *                       lpf_noc_register().
 * \param[in] src_offset The offset of reading out the source memory area,
 *                       w.r.t. the base location of the registered area
 *                       expressed in bytes.
 * \param[in] dst_slot   The memory slot of the local destination memory area
 *                       registered using lpf_register_local(),
 *                       lpf_register_global(), or lpf_noc_register().
 * \param[in] dst_offset The offset of writing to the destination memory area
 *                       w.r.t. the base location of the registered area
 *                       expressed in bytes.
 * \param[in] size       The number of bytes to copy from the source
 *                       remote memory location.
 * \param[in] attr
 *            \parblock
 *            In case an \a attr not equal to #LPF_MSG_DEFAULT is provided, the
 *            the message created by this function may have modified semantics
 *            that may be used to extend this API. Examples include:
 *
 *              -# delaying the superstep deadline of delivery, and/or
 *              -# DRMA with message combining semantics.
 *
 *            These attributes are stored after a call to this function has
 *            completed and may be modified immediately after without affecting
 *            any messages already scheduled.
 *            \endparblock
 *
 * \note See #lpf_get for notes on the use of #lpf_msg_attr_t.
 *
 * \returns #LPF_SUCCESS
 *            When the communication request was recorded successfully.
 *
 * \par BSP costs
 * This function will increase
 *   \f$ r_{c}^{(s)} \f$
 * and
 *   \f$ t_{c}^{(\mathit{pid})} \f$
 * by \a size, where c is the current superstep number and s is this process ID
 * (as provided via lpf_exec(). See \ref BSPCOSTS on how this affects real-time
 * communication costs.
 *
 * \par Runtime costs
 * See \ref BSPCOSTS.
 */
extern _LPFLIB_API
lpf_err_t lpf_noc_get(
    lpf_t ctx,
    lpf_pid_t src_pid,
    lpf_memslot_t src_slot,
    size_t src_offset,
    lpf_memslot_t dst_slot,
    size_t dst_offset,
    size_t size,
    lpf_msg_attr_t attr
);

/**
 * @}
 *
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
