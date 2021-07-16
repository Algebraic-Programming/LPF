
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

#include "dispatch.hpp"

namespace lpf { namespace hybrid {

    const Thread::err_t Thread::SUCCESS = GET_THREAD( SUCCESS );
    const Thread::err_t Thread::ERR_OUT_OF_MEMORY = GET_THREAD( ERR_OUT_OF_MEMORY );
    const Thread::err_t Thread::ERR_FATAL = GET_THREAD( ERR_FATAL );

    const Thread::args_t Thread::NO_ARGS = GET_THREAD( NO_ARGS );
    const Thread::sync_attr_t Thread::SYNC_DEFAULT = GET_THREAD( SYNC_DEFAULT );
    const Thread::msg_attr_t Thread::MSG_DEFAULT = GET_THREAD( MSG_DEFAULT );
    const Thread::pid_t Thread::MAX_P = GET_THREAD( MAX_P );

    const Thread::memslot_t Thread::INVALID_MEMSLOT = GET_THREAD( INVALID_MEMSLOT );
    const Thread::machine_t Thread::INVALID_MACHINE = GET_THREAD( INVALID_MACHINE );
    const Thread::ctx_t Thread::NONE = GET_THREAD( NONE );
    const Thread::init_t Thread::INIT_NONE = GET_THREAD( INIT_NONE );
    const Thread::ctx_t & Thread::ROOT = GET_THREAD( ROOT );

    const MPI::err_t MPI::SUCCESS = GET_MPI( SUCCESS );
    const MPI::err_t MPI::ERR_OUT_OF_MEMORY = GET_MPI( ERR_OUT_OF_MEMORY );
    const MPI::err_t MPI::ERR_FATAL = GET_MPI( ERR_FATAL );

    const MPI::args_t MPI::NO_ARGS = GET_MPI( NO_ARGS );
    const MPI::sync_attr_t MPI::SYNC_DEFAULT = GET_MPI( SYNC_DEFAULT );
    const MPI::msg_attr_t MPI::MSG_DEFAULT = GET_MPI( MSG_DEFAULT );
    const MPI::pid_t MPI::MAX_P = GET_MPI( MAX_P );

    const MPI::memslot_t MPI::INVALID_MEMSLOT = GET_MPI( INVALID_MEMSLOT );
    const MPI::machine_t MPI::INVALID_MACHINE = GET_MPI( INVALID_MACHINE );
    const MPI::ctx_t MPI::NONE = GET_MPI( NONE );
    const MPI::init_t MPI::INIT_NONE = GET_MPI( INIT_NONE );
    const MPI::ctx_t & MPI::ROOT = GET_MPI( ROOT );

} }
