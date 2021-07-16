
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

#ifndef _H_LPF_BSMP
#define _H_LPF_BSMP

#include <lpf/core.h>

#define	_LPF_BMSP_VERSION 201500L

#ifdef __cplusplus
#define restrict __restrict__
extern "C" {
#endif

/** \addtogroup LPF_HL Lightweight Parallel Foundation's higher-level libraries
 *
 * @{
 *
 * \defgroup LPF_BSMP Bulk Synchronous Message Passing API
 *
 * @{
 */

/**
 * A BSMP object can have one of the following modes at any single superstep.
 */
enum lpf_bsmp_mode {

	/** 
	 * A BSMP object that has this mode can only be read from during the
	 * current superstep. After a next call to lpf_bsmp_sync the mode switches
	 * to \a WRITE. If the next call to lpf_sync precedes that of a next call
	 * to lpf_bsmp_sync, then the mode of a BSMP object shall not change.
	 */
	READ,

	/** 
	 * A BSMP object that has this mode can only be written to during the
	 * current superstep. After a next call to lpf_bsmp_sync the mode switches
	 * to \a READ. If the next call to lpf_sync precedes that of a next call to
	 * lpf_bsmp_sync, then the mode of a BSMP object shall not change.
	 */
	WRITE
};

/** All information required by a BSMP object. */
struct lpf_bsmp_buffer {

	/** The currently active local buffer size, in bytes. */
	size_t buffer_size;

	/** The currently active tag size, in bytes. */
	size_t tag_size;

	/** The number of out-bound messages currently queued. */
	size_t queued;

	/** Array that keeps track of remote positions of messages to be sent out. */
	size_t * restrict remote_pos;

	/** Array that keeps track of local positions of messages to be read. */
	size_t * restrict local_pos;

	/** Array that holds a header info for each out-bound message. */
	size_t * restrict headers;

	/** The message inbox queue. */
	char * restrict message_queue;

	/** Globally registered slot to the message queue. */
	lpf_memslot_t in_slot;

	/** Locally registered slot to the headers. */
	lpf_memslot_t out_slot;

	/** Locally registered slot to the remote_pos array. */
	lpf_memslot_t rpos_slot;

	/** Globally registered slot to the local_pos array. */
	lpf_memslot_t lpos_slot;

	/** The process ID this BSMP buffer belongs to. */
	lpf_pid_t s;

	/** The number of processes involved in the current SPMD run. */
	lpf_pid_t p;

	/** From which process the next incoming message is to be read from. */
	lpf_pid_t pid_pos;

	/** The current mode of the BSMP queue. */
	enum lpf_bsmp_mode mode;

	/** The LPF runtime state as given via lpf_bsmp_create(). */
	lpf_t ctx;
};

/** The BSMP object type. */
#ifdef DOXYGEN
typedef ... lpf_bsmp_t;
#else
typedef struct lpf_bsmp_buffer * lpf_bsmp_t;
#endif

/** An invalid BSMP object type. */
extern _LPFLIB_API const lpf_bsmp_t LPF_INVALID_BSMP;

/** A new error code that signals a full BSMP buffer error for lpf_send(). */
extern _LPFLIB_API const lpf_err_t LPF_ERR_BSMP_FULL;

/**
 * A new error code that signals a corrupt BSMP buffer. If this error code is
 * returned, further use of the corresponding #lpf_bsmp_t object leads to
 * undefined behaviour. A next call to lpf_bsmp_sync() will  bring back the BSMP
 * buffer in a valid state.
 */
extern _LPFLIB_API const lpf_err_t LPF_ERR_BSMP_INVAL;

/** A new size_t object that corresponds to an invalid size. */
extern _LPFLIB_API const size_t LPF_INVALID_SIZE;

/** A new #lpf_memslot_t object that corresponds to an invalid memslot. */
extern _LPFLIB_API const lpf_memslot_t LPF_INVALID_MEMSLOT;

/**
 * Creates a new BSMP object. A successful call to this function will create a
 * new BSMP object that is initially in \a WRITE mode.
 *
 * @param[in]     ctx      A valid LPF context.
 * @param[in]      s       This process ID in the context of \a ctx.
 * @param[in]      p       The total number of processes running in the context
 *                         of \a ctx.
 * @param[in]  buffer_size The maximum number of payload bytes that can be
 *                         received or sent during any single superstep.
 * @param[in]   tag_size   The number of bytes each message contains in its
 *                         \a tag. Contrary to the payload of a single message,
 *                         the \a tag is always of the same size defined by
 *                         this value.
 * @param[in] max_messages The maximum number of messages this BSMP object sends
 *                         or receives during any single superstep.
 * @param[out]    bsmp     Where the resulting BSMP object should be stored.
 *
 * @returns LPF_SUCCESS           When BSMP object creation completed
 *                                successfully.
 * @returns LPF_ERR_OUT_OF_MEMORY When not enough memory could be allocated to
 *                                create a new BSMP object of the requested
 *                                size. When this is returned, \a bsmp will be
 *                                set to \a LPF_INVALID_BSMP while any
 *                                successful memory allocations are reverted;
 *                                other than setting an invalid \a bsmp it will
 *                                be as though the call to this function was
 *                                never made.
 * \parblock
 * \par LPF usage.
 * A call to this function
 *   -# registers \em four memory slots.
 *   -# schedules \em zero messages to be sent.
 *   -# expects to receive \em zero messages.
 *   -# calls #lpf_sync once (for memory registration).
 * \endparblock
 */
extern _LPFLIB_API
lpf_err_t lpf_bsmp_create(
    const lpf_t ctx,
    const lpf_pid_t s,
    const lpf_pid_t p,
    const size_t buffer_size,
    const size_t tag_size,
    const size_t max_messages,
    lpf_bsmp_t * const bsmp
);

/**
 * Destroys a valid BSMP object. After a successful call to this function, the
 * given queue is set to \a LPF_INVALID_BSMP.
 *
 * @param[in,out] bsmp The BSMP object to destroy.
 *
 * \warning If \a bsmp on input does not correspond to a valid BSMP object,
 * undefined behaviour will occur.
 *
 * @returns LPF_SUCCESS This function never fails.
 *
 * \note This relies on the specification of #lpf_deregister which also always
 *       succeeds.
 * 
 * \parblock
 * \par LPF usage.
 * A call to this function
 *   -# \em de-registers \em four LPF memory slots.
 *   -# schedules \em zero messages to be sent.
 *   -# expects to receive \em zero messages.
 *   -# does \em not call #lpf_sync.
 * \endparblock
 */
extern _LPFLIB_API
lpf_err_t lpf_bsmp_destroy( lpf_bsmp_t * const bsmp );

/**
 * Given a local BSMP buffer, schedule a message for being sent to a remote
 * BSMP object.
 *
 * @param[in] bsmp           A valid BSMP object that is in \a WRITE mode.
 * @param[in] pid            The destination process.
 * @param[in] tag            The locally or globally registered memory slot
 *                           from which to read the message tag.
 * @param[in] tag_offset     The offset from the memory slot \a tag which
 *                           indicates where the tag data resides.
 * @param[in] payload        The locally or globally registered memory slot
 *                           from which to read the message payload.
 * @param[in] payload_offset The offset from the memory slot \a payload which
 *                           indicates where the payload data resides.
 * @param[in] payload_size   The size (in bytes) of the payload data.
 *
 * \warning When \a bsmp does not correspond to a valid BSMP object, undefined
 *          behaviour will occur.
 * \warning When passing a valid \a bsmp object that is not in \a WRITE mode,
 *          undefined behaviour will occur.
 *
 * @returns LPF_SUCCESS        When the messages was successfully queued.
 * @returns LPF_ERR_BSMP_INVAL When the BSMP object is in an invalid state. In
 *                             this case the next call using \a bsmp that
 *                             \em may succeed is one to \a lpf_bsmp_sync. The
 *                             call to this function has no other effects; in
 *                             particular, the given message will not be
 *                             scheduled.
 * @returns LPF_ERR_BSMP_FULL  When the BSMP queue is full. In this case it will
 *                             be as though this call was never made.
 * 
 * \parblock
 * \par LPF usage.
 * A call to this function
 *   -# \em registers \em zero LPF memory slots.
 *   -# schedules \em three messages to be sent.
 *   -# expects to receive \em zero messages.
 *   -# does \em not call #lpf_sync.
 * \endparblock
 */
extern _LPFLIB_API
lpf_err_t lpf_send(
	const lpf_bsmp_t bsmp,
	const lpf_pid_t pid,
	const lpf_memslot_t tag,
	const size_t tag_offset,
	const lpf_memslot_t payload,
	const size_t payload_offset,
	const size_t payload_size
);

/**
 * A wrapper around the regular \a lpf_sync. This wrapper additionally
 * switches the mode of the given \a bsmp object; i.e., a BSMP object in read
 * mode will be in write mode after a call to this function, and vice versa.
 *
 * @param[in] bsmp A valid BSMP object to switch the state of.
 * @param[in] hint The #lpf_sync_attr_t that should be passed to the #lpf_sync
 *                 on the LPF context \a bsmp was created with.
 *
 * If \a bsmp was in \a READ mode, then after the call to this function \a bsmp
 * shall be in \a WRITE mode. All messages that are currently in the buffer
 * will become unavailable.
 * If \a bsmp was in \a WRITE mode, then after the call to this function
 * \a bsmp shall be in \a READ mode. All messages that were ready to send out
 * will be made available at the sibling processes after a successful call to
 * this function. Messages send to this process by sibling processes will be
 * readable by successive calls to #lpf_move after the call to this function.
 *
 * @return LPF_SUCCESS   On successful switching
 * @return LPF_ERR_FATAL If the underlying call to #lpf_sync fails. The state
 *                       of the BSMP object will become invalid after this
 *                       error is returned. The state of the LPF core library
 *                       is undefined and should no longer be relied upon.
 *
 * @see #lpf_sync for full details on the other effects of a call to this
 *                function.
 * 
 * \parblock
 * \par LPF usage.
 * A call to this function
 *   -# \em registers \em zero LPF memory slots.
 *   -# schedules \em P messages to be sent.
 *   -# expects to receive \em P messages, plus three more messages per BSMP
 *      message directed to this process. This number, while unknown, is bounded
 *      by the value of \a max_messages which was used to create the given
 *      \a bsmp object.
 *   -# \em does call #lpf_sync exactly \em once.
 * \endparblock
 */
extern _LPFLIB_API
lpf_err_t lpf_bsmp_sync( lpf_bsmp_t bsmp, lpf_sync_attr_t hint );

/**
 * A wrapper around the regular \a lpf_sync. This wrapper additionally switches
 * the mode of all #lpf_bsmp_t objects in the given array \a bsmp. This function
 * is semantically equivalent to the following implementation:
 * \code
 * lpf_err_t lpf_bsmp_syncall( lpf_bsmp_t * const bsmp, const size_t num_objects,
 *                             lpf_sync_attr_t hint ) {
 *     for( size_t i = 0; i < num_objects; ++i ) {
 *         lpf_bsmp_sync( bsmp[ i ], hint );
 *     }
 * }
 * \endcode
 * This function call is preferred since all preamble and postamble bookkeeping
 * on \a bsmp objects is fused, requiring a single call to #lpf_sync to be made,
 * instead of \a num_objects calls to #lpf_sync as in the above example.
 *
 * @param[in] bsmp Pointer to an array of \a num_objects #lpf_bsmp_t objects.
 *                 All objects must be valid.
 * @param[in] num_objects The number of #lpf_bsmp_t objects in the array
 *                        pointed to by \a num_objects.
 * @param[in] hint The hint to #lpf_sync that will be passed to the internal
 *                 call to #lpf_sync.
 *
 * @see lpf_bsmp_sync For the exact behaviour on each single BSMP object in
 *                    \a bsmp.
 * 
 * \parblock
 * \par LPF usage.
 * A call to this function
 *   -# \em registers \em zero LPF memory slots.
 *   -# schedules \em \f$ \mathit{num\_objects} \cdot P \f$ messages to be sent.
 *   -# expects to receive \em \f$ \mathit{num\_objects} \cdot P \f$  messages,
 *      plus three more messages per BSMP message directed to this process. This
 *      number is bounded by \a max_messages used to create each BSMP object.
 *   -# \em does call #lpf_sync exactly \em once.
 * \endparblock
 */
extern _LPFLIB_API
lpf_err_t lpf_bsmp_syncall( lpf_bsmp_t * const bsmp, const size_t num_objects, lpf_sync_attr_t hint );

/**
 * Given a local BSMP buffer, retrieve a message from the incoming queue.
 *
 * @param[in]  bsmp      A valid BSMP object that is in \a READ mode.
 * @param[out] tag_p     Where to store a pointer to the message tag data.
 * @param[out] payload_p Where to store a pointer to the message payload data.
 * @param[out] size      Where to store the payload size (in bytes).
 *
 * Messages that were sent to this process are received in an arbitrary order.
 * Once a message has been retrieved using a call to this function, it is
 * immediately removed from the queue. If the queue was empty while this
 * function was called, the \a size will be set to #LPF_INVALID_SIZE, while
 * \a tag_p and \a payload_p will be set to \a NULL.
 *
 * @returns LPF_SUCCESS A call to this function always succeeds.
 *
 * \warning If \a bsmp was not valid or was not in \a READ mode, undefined
 *          behaviour will occur.
 */
extern _LPFLIB_API
lpf_err_t lpf_move( lpf_bsmp_t bsmp, void * * const tag_p, void * * const payload_p, size_t * const size );

/**
 * @}
 *
 * @}
 */

#ifdef __cplusplus
} //end ``extern "C"''
#undef restrict
#endif

#endif //end ``_H_LPF_BSMP''

