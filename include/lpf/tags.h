
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

#ifndef LPFLIB_TAGS_H
#define LPFLIB_TAGS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \addtogroup LPF_EXTENSIONS LPF API extensions
 * @{
 *
 * \defgroup LPF_TAGS
 *
 * Tags enable identifying groups of messages that a call to #lpf_sync should
 * wait on. This is an extension on the classic BSP behaviour that all messages
 * issued during the communication phase of a superstep must be waited on; tags
 * instead identify potentially multiple independent communication phases.
 * Rather than #lpf_sync ending all communication phases, it may now elect to
 * end a specific communication phase only, as identified by a tag.
 *
 * This mechanism is implemented by allowing tags to be tied to LPF message
 * attributes as well as to LPF synchronisation attributes.
 *
 * @{
 */

/**
 * The specification version of the tags.
 *
 * \note It is likely that the first released version of tags will not be the
 *       first version, because the various recent extensions (non-coherent
 *       RDMA, zero-cost synchronisation, and tags) are all intricately linked.
 *       To keep the main LPF branch understandable, features will be
 *       iteratively introduced.
 */
#define LPF_TAGS_VERSION 202500L

/**
 * The type of an LPF tag.
 *
 * \par Communication
 * Objects of this type must not be communicated.
 */
#ifdef DOXYGEN
typedef ... lpf_tag_t;
#else
typedef uint32_t lpf_tag_t;
#endif

/**
 * A dummy value to initialize an #lpf_tag_t instance at declaration.
 *
 * \note A debug implementation may check for this value so that errors can be
 *       detected.
 */
extern _LPFLIB_VAR const lpf_tag_t LPF_INVALID_TAG;

/**
 * Resizes the tag register for subsequent supersteps.
 *
 * The new capacity becomes valid \em after a next call to lpf_sync(). The
 * initial capacity is zero.
 *
 * Each call to lpf_create_tag counts as one, while every valid call to
 * lpf_destroy_tag decrements the number of registered tags by one. The
 * initializer tag #LPF_INVALID_TAG does not count towards the number of
 * registered tags.
 *
 * If allocation was successful, the return value is #LPF_SUCCESS. In the case
 * of insufficient local memory, the return value is #LPF_ERR_OUT_OF_MEMORY.
 *
 * \note The current maximum nor currently registered number of tags cannot be
 *       retrieved from the run-time. Instead, the programmer must track this
 *       information herself. To provide encapsulation, please see lpf_rehook().
 *
 * A call to this function with \a max_tags smaller than the current capacity
 * shall always return #LPF_SUCCESS.
 *
 * \note When the given new capacity is smaller than the current capacity, the
 *       run-time is allowed but not required to release any superfluous
 *       memory. Implementations that do so must ensure that in case there was
 *       no space to allocate the smaller buffer, the older larger buffer
 *       remains intact (calls to this function requesting smaller-than-current
 *       capacity shall never fail).
 *
 * \par Thread safety
 * This function is safe to be called from different LPF processes only.
 *
 * \returns #LPF_SUCCESS When the process acquired resources for registering
 *                       \a max_tags tags.
 *
 * \returns #LPF_ERR_OUT_OF_MEMORY When there was not enough memory left on the
 *                                 heap. On return, the effect is the same as
 *                                 when this call did not occur at all.
 *
 * \par BSP costs
 * None
 *
 * \par Runtime costs
 * \f$ \mathcal{O}( \mathit{max\_tags} ) \f$.
 */
extern _LPFLIB_API
lpf_err_t lpf_resize_tag_register(
    lpf_t ctx,
    size_t max_tags
);

/**
 * Creates a new tag.
 *
 * This is a collective function, meaning that all processes call this
 * primitive in the same superstep and in the same order.
 *
 * Once a tag is created, it takes one tag registration slot. The maximum
 * number of registrations is given by lpf_resize_tag_register. On entering
 * this call, the user shall ensure at least one tag register remains free.
 *
 * @param[in,out] ctx  The LPF context.
 * @param[out]    tag  The resulting tag.
 *
 * @returns #LPF_SUCCESS If the creation of the tag is successful.
 */
extern _LPFLIB_API
lpf_err_t lpf_tag_create(
    lpf_t ctx,
    lpf_tag_t * tag
);

/**
 * Destroys a tag created by #lpf_tags_create.
 *
 * This is a collective function, meaning that all processes call this primitive
 * on the same tag in the same superstep and in the same order.
 *
 * @param[in,out] ctx The LPF context.
 * @param[in]     tag The tag to be destroyed.
 *
 * The given \a tag must have been the result of a previous succesful call to
 * #lpf_tags_create that was not already followed by a successful call to
 * #lpf_tags_destroy.
 *
 * After a successful call to this function, the number of registered tags
 * decreases by one.
 *
 * @returns #LPF_SUCCESS If the destruction of the tag is successful.
 */
extern _LPFLIB_API
lpf_err_t lpf_tag_destroy(
    lpf_t ctx,
    lpf_tag_t tag
);

/**
 * Retrieves a tag from a message attribute.
 *
 * @param[in,out] ctx  The LPF context.
 * @param[in]     attr The message attribute.
 * @param[out]    tag  Where to store the tag that was attached to \a attr.
 *
 * The given \a attr must have been initialized.
 *
 * \note An implementation must at least support attribute initialization via
 *       #lpf_tags_create_msg_attr.
 *
 * If \a attr was not attached a tag, then #LPF_INVALID_TAG will be returned at
 * \a tag.
 *
 * \par Thread safety
 * This function is safe to be called from different LPF processes only.
 *
 * \returns #LPF_SUCCESS A call to this function always succeeds.
 *
 * \par BSP costs
 * None
 *
 * \par Runtime costs
 * \f$ \Theta( 1 ) \f$.
 */
extern _LPFLIB_API
lpf_err_t lpf_tag_get_mattr(
    lpf_t ctx,
    lpf_msg_attr_t attr,
    lpf_tag_t * tag
);

/**
 * Attaches a tag to a given message attribute.
 *
 * @param[in,out] ctx  The LPF context.
 * @param[in]     tag  The tag to attach to \a attr.
 * @param[in,out] attr Where to attach the \a tag to.
 *
 * \TODO Extend documentation
 */
extern _LPFLIB_API
lpf_err_t lpf_tag_set_mattr(
    lpf_t ctx,
    lpf_tag_t tag,
    lpf_msg_attr_t * attr
);

/**
 * Gets a tag from a given synchronisation attribute.
 *
 * @param[in,out] ctx  The LPF context.
 * @param[in]     attr The synchronisation attribute.
 * @param[out]    tag  Where to store the tag that was attached to \a attr.
 *
 * \TODO Extend documentation
 */
extern _LPFLIB_API
lpf_err_t lpf_tag_get_sattr(
    lpf_t ctx,
    lpf_sync_attr_t attr,
    lpf_tag_t * tag
);

/**
 * Attaches a tag to a given synchronisation attribute.
 *
 * @param[in,out] ctx  The LPF context.
 * @param[in]     tag  The tag to attach to \a attr.
 * @param[in,out] attr Where to attach the \a tag to.
 *
 * \TODO Extend documentation
 */
extern _LPFLIB_API
lpf_err_t lpf_tag_set_sattr(
    lpf_t ctx,
    lpf_tag_t tag,
    lpf_sync_attr_t * attr
);

/**
 * @}
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif // LPFLIB_TAGS_H
