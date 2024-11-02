
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

#ifndef LPFLIB_ABORT_H
#define LPFLIB_ABORT_H

#include "lpf/static_dispatch.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \addtogroup LPF_EXTENSIONS LPF API extensions
 * @{
 *
 * \defgroup LPF_ABORT Provides functionality akin to the stdlib abort
 *
 * @{
 */

/**
 * Whether the selected LPF engine supports aborting distributed applications.
 *
 * If the value of this field is zero (0), then a call to #lpf_abort will be a
 * no-op and always return #LPF_SUCCESS.
 */
extern const int LPF_HAS_ABORT ;

/**
 * A call to this function aborts the distributed application as soon as
 * possible.
 *
 * \warning This function corresponds to a no-op if #LPF_MAY_ABORT equals zero.
 *
 * The below specification only applies when #LPF_MAY_ABORT contains a non-zero
 * value; otherwise, a call to this function will have no other effect besides
 * returning #LPF_SUCCESS.
 *
 * \note Rationale: the capability to abort relies on the software stack that
 *       underlies LPF, and in aiming to be a minimal API, LPF does not wish to
 *       force such a capabilities unto the underlying software or system.
 *
 * \note Applications that rely on #lpf_abort therefore should first check if
 *       the capability is supported.
 *
 * \note The recommended way to abort LPF applications that is fully supported
 *       by the core specification alone (i.e., excluding this #lpf_abort
 *       extension), is to simply exit the process that should be aborted.
 *       Compliant LPF implementations will then quit sibling processes <em>at
 *       latest</em> at a call to #lpf_sync that should handle communications
 *       with the exited process. Sibling processes may also exit early without
 *       involvement of LPF. In all cases, the parent call to #lpf_exec,
 *       #lpf_hook, or #lpf_rehook should return with #LPF_ERR_FATAL.
 *
 * \warning Therefore, whenever possible, code implemented on top of LPF ideally
 *          does not rely on #lpf_abort. Instead, error handling more reliably
 *          could be implemented on top of the above-described default LPF
 *          behaviour.
 *
 * The call to #lpf_abort diffes from the stdlib <tt>abort</tt> in that
 * implementations are not required to raise SIGABRT. The only requirements are
 * that:
 *  1. processes that call this function terminate during the call to
 *     #lpf_abort.
 *  2. all other processes associated with the distributed application terminate
 *     at latest during a next call to #lpf_sync that should have handled
 *     communications with the aborted process;
 *  3. regardless of whether LPF aborted sibling processes, whether they exited
 *     gracefully, or whether they also called #lpf_abort, the process(es) which
 *     made the parent call to #lpf_exec, #lpf_hook, or #lpf_rehook should
 *     either: a) terminate also, at latest when all (other) associated
 *     processes have terminated, (exclusive-)or b) return #LPF_ERR_FATAL.
 *     Which behaviour (a or b) will be followed is up to the implementation,
 *     and portable applications should account for both possibilities.
 *
 * \note In the above, \em other is between parenthesis since the processes
 *       executing the application may be fully disjoint from the process that
 *       spawned the application. In this case it is natural to elect that the
 *       spawning process returns #LPF_ERR_FATAL, though under this
 *       specification also that process may be aborted before the spawning
 *       call returns.
 *
 * \note If one of the associated processes deadlock (e.g. due to executing
 *       <tt>while(1){}</tt>), it shall remain undefined when the entire
 *       application aborts. Implementations shall make a best effort to do this
 *       as early as possible.
 *
 * \note Though implied by the above, we note explicitly that #lpf_abort is
 *       \em not a collective function; a single process calling #lpf_abort can
 *       terminate all associated processes.
 *
 * @returns #LPF_SUCCESS If and only if #LPF_MAY_ABORT equals zero.
 *
 * If #LPF_MAY_ABORT is nonzero, then this function shall not return.
 */
extern _LPFLIB_API 
lpf_err_t lpf_abort(lpf_t ctx);

/**
 * @}
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
