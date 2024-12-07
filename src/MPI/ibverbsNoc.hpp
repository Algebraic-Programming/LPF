#pragma once

#include "ibverbs.hpp"

namespace lpf 
{

namespace mpi 
{
    class _LPFLIB_LOCAL IBVerbsNoc : public IBVerbs {
        public:
            IBVerbsNoc(Communication & comm);
            IBVerbs::SlotID regLocal( void * addr, size_t size );
            MemoryRegistration getMR(SlotID slotId, int pid);
            void setMR(SlotID slotId, int pid, MemoryRegistration & mr);

    };
} // namespace mpi
} // namespace lpf
