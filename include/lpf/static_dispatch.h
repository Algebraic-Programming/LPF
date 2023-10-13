
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

#ifndef LPF_RENAME_PRIMITIVE4
  #define LPF_RENAME_PRIMITIVE40(p, q, r, s)  p ## _ ## q ##  _ ## r ## _  ## s
  #define LPF_RENAME_PRIMITIVE4(p, q, r, s)   LPF_RENAME_PRIMITIVE40(p, q, r, s)
#endif

#ifndef LPF_RENAME_PRIMITIVE2
  #define LPF_RENAME_PRIMITIVE20(p, s)  p ## _ ## s
  #define LPF_RENAME_PRIMITIVE2(p, s)   LPF_RENAME_PRIMITIVE20(p, s)
#endif

#undef LPF_FUNC
#undef LPF_TYPE
#undef LPF_CONST

#if defined LPF_CORE_STATIC_DISPATCH

 #define LPF_FUNC(s)  LPF_RENAME_PRIMITIVE4(lpf, LPF_CORE_STATIC_DISPATCH_ID, LPF_CORE_STATIC_DISPATCH_CONFIG, s)
 #define LPF_TYPE(s)  LPF_FUNC(s)
 #define LPF_CONST(s) LPF_RENAME_PRIMITIVE4(LPF, LPF_CORE_STATIC_DISPATCH_ID, LPF_CORE_STATIC_DISPATCH_CONFIG, s)

#endif

#undef lpf_get
#undef lpf_put
#undef lpf_sync
#undef lpf_register_local
#undef lpf_get_rcvd_msg_count
#undef lpf_get_rcvd_msg_count_per_slot
#undef lpf_register_global
#undef lpf_deregister
#undef lpf_probe
#undef lpf_resize_memory_register
#undef lpf_resize_message_queue
#undef lpf_exec
#undef lpf_hook
#undef lpf_rehook
#undef lpf_abort

#undef lpf_init_t
#undef lpf_pid_t
#undef lpf_machine_t
#undef lpf_args
#undef lpf_args_t
#undef lpf_memslot_t
#undef lpf_msg_attr_t
#undef lpf_machine
#undef lpf_machine_t
#undef lpf_sync_attr_t
#undef lpf_t
#undef lpf_spmd_t
#undef lpf_func_t
#undef lpf_err_t

#undef LPF_ROOT
#undef LPF_MAX_P
#undef LPF_SUCCESS
#undef LPF_ERR_OUT_OF_MEMORY
#undef LPF_ERR_FATAL
#undef LPF_MSG_DEFAULT
#undef LPF_SYNC_DEFAULT
#undef LPF_INVALID_MEMSLOT
#undef LPF_INVALID_MACHINE
#undef LPF_NONE
#undef LPF_INIT_NONE
#undef LPF_NO_ARGS

#ifdef LPF_FUNC

#define lpf_get             LPF_FUNC(get)
#define lpf_put             LPF_FUNC(put)
#define lpf_sync            LPF_FUNC(sync)
#define lpf_register_local  LPF_FUNC(register_local)
#define lpf_get_rcvd_msg_count LPF_FUNC(get_rcvd_msg_count)
#define lpf_get_rcvd_msg_count_per_slot LPF_FUNC(get_rcvd_msg_count_per_slot)
#define lpf_register_global LPF_FUNC(register_global)
#define lpf_deregister      LPF_FUNC(deregister)
#define lpf_probe           LPF_FUNC(probe)
#define lpf_resize_memory_register    LPF_FUNC(resize_memory_register)
#define lpf_resize_message_queue      LPF_FUNC(resize_message_queue)
#define lpf_exec            LPF_FUNC(exec)
#define lpf_hook            LPF_FUNC(hook)
#define lpf_rehook          LPF_FUNC(rehook)
#define lpf_abort           LPF_FUNC(abort)

#define lpf_init_t      LPF_TYPE(init_t)
#define lpf_pid_t       LPF_TYPE(pid_t)
#define lpf_machine_t   LPF_TYPE(machine_t)
#define lpf_args        LPF_TYPE(args)
#define lpf_args_t      LPF_TYPE(args_t)
#define lpf_memslot_t   LPF_TYPE(memslot_t)
#define lpf_msg_attr_t  LPF_TYPE(msg_attr_t)
#define lpf_machine     LPF_TYPE(machine)
#define lpf_machine_t   LPF_TYPE(machine_t)
#define lpf_sync_attr_t LPF_TYPE(sync_attr_t)
#define lpf_t           LPF_TYPE(_t)
#define lpf_spmd_t      LPF_TYPE(spmd_t)
#define lpf_func_t      LPF_TYPE(func_t)
#define lpf_err_t       LPF_TYPE(err_t)

#define LPF_ROOT              LPF_CONST(ROOT)
#define LPF_MAX_P             LPF_CONST(MAX_P)
#define LPF_SUCCESS           LPF_CONST(SUCCESS)
#define LPF_ERR_OUT_OF_MEMORY LPF_CONST(ERR_OUT_OF_MEMORY)
#define LPF_ERR_FATAL         LPF_CONST(ERR_FATAL)
#define LPF_MSG_DEFAULT       LPF_CONST(MSG_DEFAULT)
#define LPF_SYNC_DEFAULT      LPF_CONST(SYNC_DEFAULT)
#define LPF_INVALID_MEMSLOT   LPF_CONST(INVALID_MEMSLOT)
#define LPF_INVALID_MACHINE   LPF_CONST(INVALID_MACHINE)
#define LPF_NONE              LPF_CONST(NONE)
#define LPF_INIT_NONE         LPF_CONST(INIT_NONE)
#define LPF_NO_ARGS           LPF_CONST(NO_ARGS)

#endif

