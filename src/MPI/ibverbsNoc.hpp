#include "ibverbs.hpp"

namespace lpf 
{

namespace mpi 
{
    class _LPFLIB_LOCAL IBVerbsNoc : public IBVerbs {
        public:
            IBVerbsNoc(Communication & comm) : IBVerbs(comm) {}
            ~IBVerbsNoc() {}

            void putNoc( SlotID srcSlot, size_t srcOffset, 
                    int dstPid, SlotID dstSlot, size_t dstOffset, size_t size );

            void getNoc( int srcPid, SlotID srcSlot, size_t srcOffset, 
                    SlotID dstSlot, size_t dstOffset, size_t size );

            SlotID regNoc( void * addr, size_t size );

    };
} // namespace mpi
} // namespace lpf
