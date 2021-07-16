
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

#ifndef PLATFORM_COMMON_MACHINEPARAMS_HPP
#define PLATFORM_COMMON_MACHINEPARAMS_HPP


#include "lpf/core.h"
#include "linkage.hpp"

#include <vector>

#if __cplusplus >= 201103L    
#include <array>
#else
#include <tr1/array>
#endif


namespace lpf {
    
    namespace LPF_CORE_IMPL_ID { namespace LPF_CORE_IMPL_CONFIG {

    class _LPFLIB_LOCAL MachineParams 
    {
    public:
        MachineParams();

        bool isSet() const { return m_set; }

        double getLatency( size_t blockSize ) const;
        double getMessageGap( size_t blockSize ) const;

        lpf_pid_t getTotalProcs() const
        { return m_totalProcs; }

        static MachineParams execProbe( lpf_t lpf, lpf_pid_t nprocs );
        static MachineParams hookProbe( lpf_t lpf, 
                lpf_pid_t pid, lpf_pid_t nprocs );

    private:

        _LPFLIB_SPMD
        static void probe( lpf_t lpf, lpf_pid_t pid, 
                lpf_pid_t nprocs, lpf_args_t args );

        static void deleteMachineParams( void * );
        static void newMachineParamsKey();
 
        static const size_t DATAPOINTS = 7;
        static const size_t BLOCK_SIZES[ DATAPOINTS ];


        bool   m_set;
        lpf_pid_t m_totalProcs;
#if __cplusplus >= 201103L    
        std::array< size_t, DATAPOINTS > m_blockSize;
        std::array< double, DATAPOINTS > m_messageGap;
        std::array< double, DATAPOINTS > m_latency;
#else
        std::tr1::array< size_t, DATAPOINTS > m_blockSize;
        std::tr1::array< double, DATAPOINTS > m_messageGap;
        std::tr1::array< double, DATAPOINTS > m_latency;
#endif
    };

    extern _LPFLIB_LOCAL
    lpf_err_t findMachineParams( lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs,  
            size_t blockSize, double maxSeconds, double hardDeadline,
            std::vector< char > & readBuffer, std::vector< char > & writeBuffer,
            double & latency, double & messageGap );

} } 

}

#endif
