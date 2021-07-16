
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

#ifndef LPF_CORE_MPI_VIRTUAL_ALL2ALL_HPP
#define LPF_CORE_MPI_VIRTUAL_ALL2ALL_HPP

#include <cstddef>

#include "linkage.hpp"

namespace lpf { namespace mpi {


class Comm;

class _LPFLIB_LOCAL VirtualAllToAll
{
public:
    virtual ~VirtualAllToAll() {}

    virtual size_t max_size( size_t maxMsgSize ) const = 0;
    virtual void reserve( size_t number, size_t maxMsgSize ) = 0;
    virtual void clear() = 0;
    virtual bool empty() const = 0;
    virtual void send( pid_t pid, const void * msg, size_t size) = 0;
    virtual size_t recv( void * buf, size_t maxSize ) = 0;

    virtual int exchange( const Comm & comm, bool prerandomize,
           int * vote, int retry=5 ) = 0;
};


} }

#endif
