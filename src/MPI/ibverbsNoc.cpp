#include "ibverbsNoc.hpp"

namespace lpf 
{
namespace mpi
{

    size_t MemoryRegistration :: serialize(char ** buf) {
        std::stringstream ss;
        size_t bufSize = sizeof(uintptr_t) + sizeof(size_t) + 2*sizeof(uint32_t) + sizeof(int);
        *buf = new char[bufSize];
        char *ptr = *buf;
        uintptr_t addrAsUintPtr = reinterpret_cast<uintptr_t>(_addr);
        memcpy(ptr, &addrAsUintPtr, sizeof(uintptr_t));
        ptr += sizeof(uintptr_t);
        memcpy(ptr, &_size, sizeof(size_t));
        ptr += sizeof(size_t);
        memcpy(ptr, &_lkey, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        memcpy(ptr, &_rkey, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        memcpy(ptr, &_pid, sizeof(int));
        return bufSize;
    }

    MemoryRegistration * MemoryRegistration :: deserialize(char * buf) {

        char *   addr;
        size_t   size;
        uint32_t lkey;
        uint32_t rkey;
        uintptr_t addrAsUintPtr;
        int pid;
        char * ptr = buf;
        memcpy(&addrAsUintPtr, ptr, sizeof(uintptr_t));
        addr = reinterpret_cast<char *>(addrAsUintPtr);
        ptr += sizeof(uintptr_t);
        memcpy(&size, ptr, sizeof(size_t));
        ptr += sizeof(size_t);
        memcpy(&lkey, ptr, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        memcpy(&rkey, ptr, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        memcpy(&pid, ptr, sizeof(int));
        return new MemoryRegistration(addr, size, lkey, rkey, pid);
    }

    struct IBVerbsNoc::Exception : std::runtime_error {
        Exception(const char * what) : std::runtime_error( what ) {}
    };

    MemoryRegistration IBVerbsNoc :: getMR(SlotID slotId, int pid) 
    {
        const MemorySlot & slot = m_memreg.lookup( slotId );
        return slot.glob[pid];
    }

    void IBVerbsNoc::setMR(SlotID slotId, int pid, MemoryRegistration & mr)
    {
        m_memreg.update(slotId).glob[pid] = mr;
    }

    IBVerbsNoc::IBVerbsNoc(Communication & comm) : IBVerbs(comm)
    {
    }

    IBVerbs::SlotID IBVerbsNoc :: regNoc( void * addr, size_t size )
    {
        ASSERT( size <= m_maxRegSize );

        MemorySlot slot;
        if ( size > 0) {
            LOG(4, "IBVerbsNoc::regLocal: Registering locally memory area at " << addr << " of size  " << size );
            struct ibv_mr * const ibv_mr_new_p = ibv_reg_mr(
                    m_pd.get(), addr, size,
                    IBV_ACCESS_REMOTE_READ | IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE
                    );
            if( ibv_mr_new_p == NULL )
                slot.mr.reset();
            else
                slot.mr.reset( ibv_mr_new_p, ibv_dereg_mr );
            if (!slot.mr) {
                LOG(1, "Could not register memory area at "
                        << addr << " of size " << size << " with IB device");
                throw Exception("Could not register memory area");
            }
        }
        MemoryRegistration local((char *) addr, size, size?slot.mr->lkey:0, size?slot.mr->rkey:0, m_pid);

        SlotID id =  m_memreg.addNocReg( slot );
        m_memreg.update( id ).glob.resize( m_nprocs );
        m_memreg.update( id ).glob[m_pid] = local;
        LOG(4, "Memory area " << addr << " of size " << size << " has been locally registered as NOC slot. Slot = " << id );
        return id;
    }

} // namespace mpi
} // namespace lpf
