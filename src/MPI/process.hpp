
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

#ifndef LPF_CORE_MPI_PROCESS_HPP
#define LPF_CORE_MPI_PROCESS_HPP

#include "mpilib.hpp"
#include "symbol.hpp"
#include "linkage.hpp"

namespace lpf {

class _LPFLIB_LOCAL Process
{
public:
    explicit Process(const mpi::Comm & comm);
    ~Process();

    pid_t nprocs() const
    { return m_world.nprocs() ; }

    pid_t pid() const
    { return m_world.pid() ; }

    err_t exec( pid_t P, spmd_t spmd, args_t args ) ;

    static err_t hook( const mpi::Comm & comm, Process & subprocess,
            spmd_t spmd, args_t args ) ;

private:
    Process( const Process & ); // copying prohibited
    Process & operator=( const Process & ) ; // assignment prohibited

    // main event-loop for all slave processes
    void slave();

    // broadcast a symbol from the root.
    static void broadcastSymbol(
            Communication & comm, 
            Symbol & symbol );

    static const pid_t ROOT = 0;

    mpi::Comm m_world;
    bool m_aborted;
};




} // namespace lpf

#endif
