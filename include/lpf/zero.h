
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
 * 1997 [1]. It is rooted in the idea that if BSP-type programs annotate how
 * many bytes are expected to be sent and received as part of a given
 * communication phase. If network interfaces can keep track of processed
 * incoming resp. outgoing bytes, then processes need only query its local
 * network interface to determine whether a superstep has completed; thus
 * avoiding the need for either collectives or barriers.
 *
 * This extension provides a variant of zero-cost synchronisation that is based
 * on counting the number of messages rather than number of bytes. It is
 * compatible with the concept of a \em tag; see \ref LPF_TAGS.
 *
 * [1] Alpert, R. and Philbin, J., 1997. cBSP: Zero-cost synchronization in a
 *     modified BSP model. NEC Research Institute, Princeton, NJ, USA,
 *     Tech. Rep, pp.97-054.
 */

/**
 * The specification version of zero-cost synchronisation.
 *
 * \note It is likely that the first released version will not be the first
 *       version, because the various recent extensions (non-coherent RDMA,
 *       zero-cost synchronisation, and tags) are all intricately linked. To
 *       keep the main LPF branch understandable, features will be
 *       iteratively introduced.
 */
#define LPF_ZERO_COST_SYNC 202500L

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
 * If the resulting \a attr is used within a subsequent call to #lpf_sync,
 * the spec demands that the #lpf_sync call is collective. The zero-cost
 * synchronisation extension furthermore requires that each of those collective
 * calls to #lpf_sync have matching zero-cost attributes attached to them. Here,
 * ``matching'' means that the combination of all attributes given at all
 * processes correctly corresponds to the global communication pattern that that
 * #lpf_sync requires wait completion for.
 *
 * @returns #LPF_SUCCESS If the attachment of the zero-cost synchronisation
 *                       attributes is successful.
 */
extern _LPFLIB_API
lpf_err_t lpf_zero_expect(
    lpf_t ctx,
    size_t expected_sent, size_t expected_rcvd,
    lpf_sync_attr_t * attr
);

/**
 * Retrieves the current locally-received number of messages.
 *
 * \TODO extend documentation
 *
 * \note Rationale: this function is useful for implementing task-aware
 *       interfaces around zero-cost synchronisation mechanisms.
 */
extern _LPFLIB_API
lpf_err_t lpf_zero_get_rcvd( lpf_t ctx, lpf_sync_attr_t attr, size_t * rcvd );

/**
 * Retrieves the current locally-sent number of messages.
 *
 * \TODO extend documentation
 *
 * \note Rationale: this function is useful for implementing task-aware
 *       interfaces around zero-cost synchronisation mechanisms.
 */
extern _LPFLIB_API
lpf_err_t lpf_zero_get_sent( lpf_t ctx, lpf_sync_attr_t attr, size_t * sent );

/**
 * @}
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif // LPFLIB_ZERO_H
