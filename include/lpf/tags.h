
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
typedef lpf_memslot_t lpf_tag_t;
#endif

/**
 * Creates a new tag.
 *
 * The tag requires a globally unique memory area, for which we re-use the LPF
 * memory slot concept.
 *
 * This is a collective function, meaning that all processes call this
 * primitive on the same global memory slot, in the same superstep, and in the
 * same order.
 *
 * @param[in,out] ctx  The LPF context.
 * @param[in]     slot A globally unique memory area used for slot creation.
 * @param[out]    tag  The resulting tag.
 *
 * The given \a slot must not have been used by a previous successful call to
 * #lpf_tags_create that was not followed by a successful call to
 * #lpf_tags_destroy.
 *
 * @returns #LPF_SUCCESS If the creation of the tag is successful.
 */
extern _LPFLIB_API
lpf_err_t lpf_tags_create(
    lpf_t ctx,
    lpf_memslot_t slot,
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
 * @returns #LPF_SUCCESS If the destruction of the tag is successful.
 */
extern _LPFLIB_API
lpf_err_t lpf_tags_destroy(
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
 * \TODO extend documentation
 */
extern _LPFLIB_API
lpf_err_t lpf_tags_get_mattr(
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
lpf_err_t lpf_tags_set_mattr(
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
lpf_err_t lpf_tags_get_sattr(
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
lpf_err_t lpf_tags_set_sattr(
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
