
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

#ifndef LPFLIB_CONFIG_HPP
#define LPFLIB_CONFIG_HPP

#include "lpf/core.h"
#include "linkage.hpp"
#include "suppressions.h"
#include <stdexcept>

namespace lpf {

    class _LPFLIB_LOCAL Config
    {
    public:
        LPFH_IGNORE_WEAK_VTABLES
        struct ReadError : public std::runtime_error {
            ReadError(const char * );
        };
        LPFH_RESTORE_WARNINGS

        static Config & instance();

        enum SpinMode { SPIN_FAST, SPIN_PAUSE, SPIN_YIELD, SPIN_COND };
        SpinMode getSpinMode() const;

        lpf_pid_t   getSysProcs() const;

        size_t getLocalRamSize() const;

        /// the maximum allowed number of processes/threads to use.
        /// is zero when values wasn't set
        lpf_pid_t   getMaxProcs() const;

        /// the pinning bitmap
        std::string getPinBitmap() const;

        /// the maximum number of seconds to spend on probing the machine
        double getMaxSecondsForProbe() const;

        /// On what level diagnostic messages should be output
        int getLogLevel() const;

        /// BSPlib emulation safety mode
        int getBSPlibSafetyMode() const;

        /// Number of memory registrations that BSPlib emulation may do for hp messages
        size_t getBSPlibMaxHpRegs() const;

        /// Get the current active engine
        std::string getEngine() const;

        enum  A2AMode { A2A_DENSE, A2A_SPARSE, A2A_HOEFLER };
        /// Get the all-to-all mode
        A2AMode getA2AMode() const;

        struct TinyMsgSize { size_t sizeForSharedMem, sizeForMPI; };
        // get the number of bytes for what is regarded as a tiny message
        TinyMsgSize getTinyMsgSize() const;

        // get the log2 of the block size in bytes of the write conflict resolution
        int getMpiMsgSortGrainSizePower() const;

        // get the maximum size for an MPI message
        size_t getMaxMPIMsgSize() const;

        std::string getIBDeviceName() const;
        int         getIBPort() const;
        int         getIBGidIndex() const;
        unsigned    getIBMTU() const;
    }; 
    

}


#endif
