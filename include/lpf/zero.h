
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

#ifndef LPFLIB_ZERO_H
#define LPFLIB_ZERO_H

#ifdef __cplusplus
extern "C" {
#endif

/** \addtogroup LPF_EXTENSIONS LPF API extensions
 * @{
 *
 * \defgroup LPF_ZERO_COST_SYNC
 *
 * This extension provides so-called <em>zero-cost synchronisation</em>
 * mechanisms on top of LPF. This term was coined by Alpert and Philbin back in
 * 1997 [1]. It is rooted in the idea that BSP programs annotate how many bytes
 * are expected to be sent and received as part of a given communication phase.
 * If, simultaneously, network interfaces can keep track of processed incoming,
 * respectively, outgoing bytes, then processes need only query its local
 * network interface to determine whether a superstep has completed-- thus
 * avoiding the need for either collectives or barriers.
 *
 * This extension provides a variant of zero-cost synchronisation that is based
 * on counting the number of messages rather than number of bytes. It is
 * compatible with the concept of a \em tag; see \ref LPF_TAGS.
 *
 * [1] Alpert, R. and Philbin, J., 1997. cBSP: Zero-cost synchronization in a
 *     modified BSP model. NEC Research Institute, Princeton, NJ, USA,
 *     Tech. Rep, pp.97-054.
 *
 * @{
 */

/**
 * The specification version of zero-cost synchronisation.
 */
#define LPF_ZERO_COST_SYNC 202500L

/**
 * Creates a new message attribute that is compatible with the LPF zero-cost
 * synchronisation extension.
 *
 * If an implementation supports additional extensions that employ message
 * attributes, then attributes initialised by this extension must result in a
 * valid message attribute for use with those other extensions also.
 *
 * \note This does \em not imply that using message attributes from multiple
 *       extensions simultaneously always yields sensible behaviour; this
 *       depends on the specification of the extensions.
 *
 * This extension is compatible with the tags extension.
 *
 * @param[in,out] ctx  The LPF context.
 * @param[out]    attr Where a new message attribute will be allocated.
 *
 * After a successful function call, applying the returned \a attr without
 * modification shall induce the same behaviour as applying #LPF_MSG_DEFAULT.
 *
 * \par Thread safety
 * This function is safe to be called from different LPF processes only.
 *
 * \returns #LPF_SUCCESS When a new \a attr was successfully constructed. After
 *                       the call to this function, the attribute pointed to by
 *                       \a attr shall be a valid message attribute.
 *
 * \returns #LPF_ERR_OUT_OF_MEMORY When not enough system resources were
 *                                 available to create a new message attribute.
 *
 * \par BSP costs
 * None.
 *
 * \par Runtime costs
 * \f$ \Theta( 1 ) \f$.
 */
extern _LPFLIB_API
lpf_err_t lpf_zero_create_mattr(
    lpf_t ctx,
    lpf_msg_attr_t * attr
);

/**
 * Creates a new synchronization attribute that is compatible with the LPF
 * zero-cost synchronization extension.
 *
 * If an implementation supports additional extensions that employ
 * synchronization attributes, then attributes initialised by this extension
 * must result in a valid synchronization attribute for use with those other
 * extensions also.
 *
 * \note This does \em not imply that using synchronization attributes from
 *       multiple extensions simultaneously always yields sensible behaviour;
 *       this depends on the specification of the extensions.
 *
 * This extension is compatible with the tags extension.
 *
 * @param[in,out] ctx  The LPF context.
 * @param[out]    attr Where a new message attribute will be allocated.
 *
 * After a successful function call, applying the returned \a attr without
 * modification shall induce the same behaviour as applying #LPF_MSG_DEFAULT.
 *
 * \par Thread safety
 * This function is safe to be called from different LPF processes only.
 *
 * \returns #LPF_SUCCESS When a new \a attr was successfully constructed. After
 *                       the call, the attribute pointed to by \a attr shall be
 *                       a valid synchronisation attribute.
 *
 * \returns #LPF_ERR_OUT_OF_MEMORY When not enough system resources were
 *                                 available to create a new message attribute.
 *
 * \par BSP costs
 * None.
 *
 * \par Runtime costs
 * \f$ \Theta( 1 ) \f$.
 */
extern _LPFLIB_API
lpf_err_t lpf_zero_create_sattr(
    lpf_t ctx,
    lpf_sync_attr_t * attr
);

/**
 * Destroys a valid message attribute.
 *
 * The given \a attr must \em not equal #LPF_MSG_DEFAULT (the default message
 * attribute may not be destroyed). The given \a attr must be created by this
 * extension \em or by an extension that is compatible with the tags extension.
 *
 * This function may be called on message attributes created by the tags
 * extension.
 *
 * @param[in,out] ctx  The LPF context.
 * @param[out]    attr The message attribute to be destroyed.
 *
 * After a successful function call, the given \a attr shall become invalid and
 * must not be used in subsequent calls to any LPF primitive.
 *
 * \par Thread safety
 * This function is safe to be called from different LPF processes only.
 *
 * \returns #LPF_SUCCESS A call to this function always succeeds.
 *
 * \par BSP costs
 * None.
 *
 * \par Runtime costs
 * \f$ \Theta( 1 ) \f$.
 */
extern _LPFLIB_API
lpf_err_t lpf_zero_destroy_mattr(
    lpf_t ctx,
    lpf_msg_attr_t attr
);

/**
 * Destroys a valid synchronization attribute.
 *
 * The given \a attr must \em not equal #LPF_SYNC_DEFAULT (the default
 * synchronization attribute may not be destroyed). The given \a attr must be
 * created by this extension \em or by an extension that is compatible with the
 * tags extension.
 *
 * This function may be called on synchronization attributes created by the tags
 * extension.
 *
 * @param[in,out] ctx  The LPF context.
 * @param[out]    attr The message attribute to be destroyed.
 *
 * After a successful function call, the given \a attr shall become invalid and
 * must not be used in subsequent calls to any LPF primitive.
 *
 * \par Thread safety
 * This function is safe to be called from different LPF processes only.
 *
 * \returns #LPF_SUCCESS A call to this function always succeeds.
 *
 * \par BSP costs
 * None.
 *
 * \par Runtime costs
 * \f$ \Theta( 1 ) \f$.
 */
extern _LPFLIB_API
lpf_err_t lpf_zero_destroy_sattr(
    lpf_t ctx,
    lpf_sync_attr_t attr
);

/**
 * Attaches zero-cost synchronisation attributes to the given LPF
 * synchronisation attribute.
 *
 * @param[in,out] ctx       The LPF context.
 * @param[in] expected_sent The expected number of messages sent out from this
 *                          process.
 * @param[in] expected_rcvd The expected number of messages received at this
 *                          process.
 * @param[in,out] attr      Where to attach the zero-cost sync attributes.
 *
 * The given \a attr must have been created via #lpf_zero_create_sattr or must
 * be created by another extension that is compatible with this zero-cost
 * synchronization extension.
 *
 * If the resulting \a attr is used within a subsequent call to #lpf_sync,
 * the spec demands that the #lpf_sync call is collective. The zero-cost
 * synchronisation extension furthermore requires that each of those collective
 * calls to #lpf_sync have matching zero-cost attributes attached to them. Here,
 * ``matching'' means that the combination of all attributes given at all
 * processes correctly corresponds to the global communication pattern that that
 * #lpf_sync requires wait completion for.
 *
 * \par Thread safety
 * This function is safe to be called from different LPF processes only.
 *
 * \returns #LPF_SUCCESS If the attachment of the zero-cost synchronisation
 *                       attributes is successful.
 *
 * \par BSP costs
 * None.
 *
 * \par Runtime costs
 * \f$ \Theta( 1 ) \f$.
 */
extern _LPFLIB_API
lpf_err_t lpf_zero_set_expected(
    lpf_t ctx,
    size_t expected_sent, size_t expected_rcvd,
    lpf_sync_attr_t attr
);

/**
 * Retrieves the attached zero-cost information from the given synchronisation
 * attribute.
 *
 * @param[in,out] ctx           The LPF context.
 * @param[in]     attr          The synchronisation attribute to retrieve the
 *                              zero-cost attributes from.
 * @param[out]    expected_sent Where to store the expected number of sent
 *                              messages.
 * @param[out]    expected_rcvd Where to store the expected number of received
 *                              messages.
 *
 * The given \a attr must have been created via #lpf_zero_create_sattr or must
 * be created by another extension that is compatible with this zero-cost
 * synchronization extension.
 *
 * If \a attr did not have a preceding call to #lpf_zero_set_expected, then the
 * default values (0) are returned. An expected zero for both received and sent
 * number of messages indicates a regular (non zero-cost) synchronization.
 *
 * \par Thread safety
 * This function is safe to be called from different LPF processes only.
 *
 * \returns #LPF_SUCCESS A call to this function always succeeds.
 *
 * \par BSP costs
 * None.
 *
 * \par Runtime costs
 * \f$ \Theta( 1 ) \f$.
 */
extern _LPFLIB_API
lpf_err_t lpf_zero_get_expected(
    lpf_t ctx,
    lpf_sync_attr_t attr,
    size_t * expected_sent, size_t * expected_rcvd
);

/**
 * Retrieves the current locally-received number of messages.
 *
 * @param[in,out] ctx           The LPF context.
 * @param[in]     attr          The synchronisation attribute to retrieve the
 *                              status of.
 * @param[out]    rcvd          Where to store the number of received messages.
 * @param[out]    sent          Where to store the number of sent messages.
 *
 * The given \a attr must have been created via #lpf_zero_create_sattr or must
 * be created by another extension that is compatible with this zero-cost
 * synchronization extension.
 *
 * \note Rationale: this function is useful for implementing task-aware
 *       interfaces around zero-cost synchronisation mechanisms.
 *
 * \par Thread safety
 * This function is safe to be called from different LPF processes only.
 *
 * \returns #LPF_SUCCESS A call to this function always succeeds.
 *
 * \par BSP costs
 * None.
 *
 * \par Runtime costs
 * \f$ \Theta( 1 ) \f$.
 *
 * \note A call to this function may imply querying the network interface,
 *       and hence the constant-time factor of a call to this function may be
 *       non-trivial; use of this function is recommended to be sparingly.
 */
extern _LPFLIB_API
lpf_err_t lpf_zero_get_status(
    lpf_t ctx, lpf_sync_attr_t attr,
    size_t * rcvd, size_t * sent
);

/**
 * @}
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif // LPFLIB_ZERO_H
