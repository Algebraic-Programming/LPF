
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

#include "memcopy.hpp"
#include <limits>


namespace lpf {

    
MemCopy :: MemCopy( )
    : m_srcPid( std::numeric_limits<pid_t>::max() )
    , m_srcSlot( std::numeric_limits<pid_t>::max() )
    , m_dstSlot( std::numeric_limits<pid_t>::max() )
    , m_srcOffset( 0 ), m_dstOffset( 0 )
    , m_size( std::numeric_limits<pid_t>::max() )
{}

MemCopy :: MemCopy( pid_t srcPid,
        memslot_t srcSlot, size_t srcOffset, 
        memslot_t dstSlot, size_t dstOffset, 
        size_t size )
    : m_srcPid( srcPid )
    , m_srcSlot( srcSlot ), m_dstSlot( dstSlot )
    , m_srcOffset( srcOffset ), m_dstOffset( dstOffset )
    , m_size( size )
{}

} // namespace lpf
