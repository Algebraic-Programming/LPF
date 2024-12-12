
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

#include "ibverbs.hpp"
#include "log.hpp"
#include "communication.hpp"
#include "config.hpp"

#include <stdexcept>
#include <cstring>
#include <unistd.h>
#include <algorithm>

#define POLL_BATCH 64
#define MAX_POLLING 128
#define ARRAY_SIZE 1000


namespace lpf { namespace mpi {


struct IBVerbs::Exception : std::runtime_error {
    Exception(const char * what) : std::runtime_error( what ) {}
};

namespace {
    ibv_mtu getMTU( unsigned  size ) {
        switch (size) {
            case 256: return IBV_MTU_256;
            case 512: return IBV_MTU_512;
            case 1024: return IBV_MTU_1024;
            case 2048: return IBV_MTU_2048;
            case 4096: return IBV_MTU_4096;
            default: throw IBVerbs::Exception("Illegal MTU size");
        }
        return IBV_MTU_4096;
    }
}


IBVerbs :: IBVerbs( Communication & comm )
    : m_comm( comm )
    , m_pid( comm.pid() )
    , m_nprocs( comm.nprocs() )
    , m_numMsgs(0)
    , m_recvTotalInitMsgCount(0)
    , m_sentMsgs(0)
    , m_recvdMsgs(0)
    , m_devName()
    , m_ibPort( Config::instance().getIBPort() )
    , m_gidIdx( Config::instance().getIBGidIndex() )
    , m_mtu( getMTU( Config::instance().getIBMTU() ))
    , m_maxRegSize(0)
    , m_maxMsgSize(0)
    , m_cqSize(1)
    , m_minNrMsgs(0)
    , m_maxSrs(0)
    , m_device()
    , m_pd()
    , m_cqLocal()
    , m_cqRemote()
    , m_stagedQps( m_nprocs )
    , m_connectedQps( m_nprocs )
    , m_srs()
    , m_srsHeads( m_nprocs, 0u )
    , m_nMsgsPerPeer( m_nprocs, 0u )
    , m_activePeers(0, m_nprocs)
    , m_peerList()
    , m_sges()
    , m_memreg()
    , m_dummyMemReg()
    , m_dummyBuffer()
    , m_postCount(0)
    , m_recvCount(0)
{

    // arrays instead of hashmap for counters
    m_recvInitMsgCount.resize(ARRAY_SIZE, 0);
    m_getInitMsgCount.resize(ARRAY_SIZE, 0);
    m_sendInitMsgCount.resize(ARRAY_SIZE, 0);
    rcvdMsgCount.resize(ARRAY_SIZE, 0);
    getMsgCount.resize(ARRAY_SIZE, 0);
    sentMsgCount.resize(ARRAY_SIZE, 0);
    slotActive.resize(ARRAY_SIZE, 0);


    m_peerList.reserve( m_nprocs );

    int numDevices = -1;
    struct ibv_device * * const try_get_device_list = ibv_get_device_list( &numDevices );

    if (!try_get_device_list) {
        LOG(1, "Cannot get list of Infiniband devices" );
        throw Exception( "failed to get IB devices list");
    }

    shared_ptr< struct ibv_device * > devList(
            try_get_device_list,
            ibv_free_device_list );

    LOG(3, "Retrieved Infiniband device list, which has " << numDevices
            << " devices"  );

    if (numDevices < 1) {
        LOG(1, "There are " << numDevices << " Infiniband devices"
                " available, which is not enough" );
        throw Exception( "No Infiniband devices available" );
    }


    std::string wantDevName = Config::instance().getIBDeviceName();
    LOG( 3, "Searching for device '"<< wantDevName << "'" );
    struct ibv_device * dev = NULL;
    for (int i = 0; i < numDevices; i ++)
    {
        std::string name = ibv_get_device_name( (&*devList)[i]);
        LOG(3, "Device " << i << " has name '" << name << "'" );
        if ( wantDevName.empty() || name == wantDevName ) {
            LOG(3, "Found device '" << name << "'" );
            m_devName = name;
            dev = (&*devList)[i];
            break;
        }
    }

    if (dev == NULL) {
        LOG(1, "Could not find device '" << wantDevName << "'" );
        throw Exception("Infiniband device not found");
    }

    struct ibv_context * const ibv_context_new_p = ibv_open_device(dev);
    if( ibv_context_new_p == NULL )
        m_device.reset();
    else
        m_device.reset( ibv_context_new_p, ibv_close_device );
    if (!m_device) {
        LOG(1, "Failed to open Infiniband device '" << m_devName << "'");
        throw Exception("Cannot open IB device");
    }
    LOG(3, "Opened Infiniband device '" << m_devName << "'" );

    devList.reset();
    LOG(3, "Closed Infiniband device list" );

    std::memset(&m_deviceAttr, 0, sizeof(m_deviceAttr));
    if (ibv_query_device( m_device.get(), &m_deviceAttr ))
        throw Exception("Cannot query device");

    LOG(3, "Queried IB device capabilities" );

    m_maxRegSize = m_deviceAttr.max_mr_size;
    LOG(3, "Maximum size for memory registration = " << m_maxRegSize );

    // maximum number of work requests per Queue Pair
    m_maxSrs = std::min<size_t>( m_deviceAttr.max_qp_wr, // maximum work requests per QP
                                 m_deviceAttr.max_cqe ); // maximum entries per CQ
    LOG(3, "Maximum number of send requests is the minimum of "
            << m_deviceAttr.max_qp_wr << " (the maximum of work requests per QP)"
            << " and " << m_deviceAttr.max_cqe << " (the maximum of completion "
            << " queue entries per QP), nameley " << m_maxSrs );

    if ( m_deviceAttr.max_cqe < m_nprocs )
        throw Exception("Completion queue has insufficient completion queue capabilities");

    struct ibv_port_attr port_attr; std::memset( &port_attr, 0, sizeof(port_attr));
    if (ibv_query_port( m_device.get(), m_ibPort, & port_attr ))
        throw Exception("Cannot query IB port");

    LOG(3, "Queried IB port " << m_ibPort << " capabilities" );

    // store Maximum message size
    m_maxMsgSize = port_attr.max_msg_sz;
    LOG(3, "Maximum IB message size is " << m_maxMsgSize );

    size_t sysRam = Config::instance().getLocalRamSize();
    m_minNrMsgs = sysRam  / m_maxMsgSize;
    LOG(3, "Minimum number of messages to allocate = "
            "total system RAM / maximum message size = "
            <<  sysRam << " / " << m_maxMsgSize << " = "  << m_minNrMsgs );

    // store LID
    m_lid = port_attr.lid;
    LOG(3, "LID is " << m_lid );

    struct ibv_pd * const pd_new_p = ibv_alloc_pd( m_device.get() );
    if( pd_new_p == NULL )
        m_pd.reset();
    else
        m_pd.reset( pd_new_p, ibv_dealloc_pd );
    if (!m_pd) {
        LOG(1, "Could not allocate protection domain ");
        throw Exception("Could not allocate protection domain");
    }
    LOG(3, "Opened protection domain");

    m_cqLocal.reset(ibv_create_cq( m_device.get(), 1, NULL, NULL, 0 ), ibv_destroy_cq);
    m_cqRemote.reset(ibv_create_cq( m_device.get(), m_nprocs, NULL, NULL, 0 ), ibv_destroy_cq);
    /**
     * New notification functionality for HiCR
     */
    struct ibv_srq_init_attr srq_init_attr;
	srq_init_attr.srq_context = NULL;
	srq_init_attr.attr.max_wr =  m_deviceAttr.max_srq_wr;
	srq_init_attr.attr.max_sge = m_deviceAttr.max_srq_sge;
	srq_init_attr.attr.srq_limit = 0;
	m_srq.reset(ibv_create_srq(m_pd.get(), &srq_init_attr ),
			ibv_destroy_srq);


    m_cqLocal.reset(ibv_create_cq( m_device.get(), m_cqSize, NULL, NULL, 0), ibv_destroy_cq);
    if (!m_cqLocal) {
        LOG(1, "Could not allocate completion queue with '"
                << m_nprocs << " entries" );
        throw Exception("Could not allocate completion queue");
    }
    m_cqRemote.reset(ibv_create_cq( m_device.get(), m_cqSize * m_nprocs, NULL, NULL, 0), ibv_destroy_cq);
    if (!m_cqLocal) {
        LOG(1, "Could not allocate completion queue with '"
                << m_nprocs << " entries" );
        throw Exception("Could not allocate completion queue");
    }

    LOG(3, "Allocated completion queue with " << m_nprocs << " entries.");

    // allocate dummy buffer
    m_dummyBuffer.resize( 8 );
    struct ibv_mr * const ibv_reg_mr_new_p = ibv_reg_mr(
        m_pd.get(), m_dummyBuffer.data(), m_dummyBuffer.size(),
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE
    );
    if( ibv_reg_mr_new_p == NULL )
        m_dummyMemReg.reset();
    else
        m_dummyMemReg.reset( ibv_reg_mr_new_p, ibv_dereg_mr );
    if (!m_dummyMemReg) {
        LOG(1, "Could not register memory region");
        throw Exception("Could not register memory region");
    }

    LOG(3, "Queue pairs have been successfully initialized");

}

IBVerbs :: ~IBVerbs()
{ }


inline void IBVerbs :: tryIncrement(Op op, Phase phase, SlotID slot) {
    
    switch (phase) {
        case Phase::INIT:
            rcvdMsgCount[slot] = 0;
            getMsgCount[slot] = 0;
            m_recvInitMsgCount[slot] = 0;
            m_getInitMsgCount[slot] = 0;
            sentMsgCount[slot] = 0;
            m_sendInitMsgCount[slot] = 0;
            slotActive[slot] = true;
            break;
        case Phase::PRE:
            if (op == Op::SEND) {
                m_numMsgs++;
                //m_sendTotalInitMsgCount++;
                m_sendInitMsgCount[slot]++;
            }
            if (op == Op::RECV) {
                m_recvTotalInitMsgCount++;
                m_recvInitMsgCount[slot]++;
            }
            if  (op == Op::GET) {
                m_recvTotalInitMsgCount++;
                m_getInitMsgCount[slot]++;
            }
            break;
        case Phase::POST:
            if (op == Op::RECV) {
                m_recvdMsgs ++;
                rcvdMsgCount[slot]++;
            }
            if (op == Op::GET) {
                m_recvdMsgs++;
                getMsgCount[slot]++;
            }
            if (op == Op::SEND) {
                m_sentMsgs++;
                sentMsgCount[slot]++;
            }
            break;
    }
}

void IBVerbs :: stageQPs( size_t maxMsgs )
{
    // create the queue pairs
    for ( size_t i = 0; i < static_cast<size_t>(m_nprocs); ++i) {
        struct ibv_qp_init_attr attr;
        std::memset(&attr, 0, sizeof(attr));

        attr.qp_type = IBV_QPT_RC; // we want reliable connection
        attr.sq_sig_all = 0; // only wait for selected messages
        attr.send_cq = m_cqLocal.get();
        attr.recv_cq = m_cqRemote.get();
        attr.srq = m_srq.get();
        attr.cap.max_send_wr = std::min<size_t>(maxMsgs + m_minNrMsgs,m_maxSrs/4);
        attr.cap.max_recv_wr = std::min<size_t>(maxMsgs + m_minNrMsgs,m_maxSrs/4);
        attr.cap.max_send_sge = 1;
        attr.cap.max_recv_sge = 1;

        struct ibv_qp * const ibv_new_qp_p = ibv_create_qp( m_pd.get(), &attr );
        ASSERT(m_stagedQps.size() > i);
        if( ibv_new_qp_p == NULL ) {
            m_stagedQps[i].reset();
        } else {
            m_stagedQps[i].reset( ibv_new_qp_p, ibv_destroy_qp );
        }
        if (!m_stagedQps[i]) {
            LOG( 1, "Could not create Infiniband Queue pair number " << i );
            throw std::bad_alloc();
        }

        LOG(3, "Created new Queue pair for " << m_pid << " -> " << i << " with qp_num = " << ibv_new_qp_p->qp_num);
    }
}

void IBVerbs :: doRemoteProgress() {
	struct ibv_wc wcs[POLL_BATCH];
	struct ibv_recv_wr wr;
	struct ibv_sge sg;
	struct ibv_recv_wr *bad_wr;
	sg.addr = (uint64_t) NULL;
	sg.length = 0;
	sg.lkey = 0;
	wr.next = NULL;
	wr.sg_list = &sg;
	wr.num_sge = 0;
	wr.wr_id = 66;
	int pollResult, totalResults = 0;
	do {
		pollResult = ibv_poll_cq(m_cqRemote.get(), POLL_BATCH, wcs);
        if (pollResult > 0) {
            LOG(3, "Process " << m_pid << " signals: I received " << pollResult << " remote messages in doRemoteProgress");
        }  
        else if (pollResult < 0)
        {
            LOG( 1, "Failed to poll IB completion queue" );
            throw Exception("Poll CQ failure");
        }

		for(int i = 0; i < pollResult; i++) {
            if (wcs[i].status != IBV_WC_SUCCESS) {
                LOG( 2, "Got bad completion status from IB message."
                        " status = 0x" << std::hex << wcs[i].status
                        << ", vendor syndrome = 0x" << std::hex
                        << wcs[i].vendor_err );
            }
            else {
                LOG(2, "Process " << m_pid << " Recv wcs[" << i << "].src_qp = "<< wcs[i].src_qp);
                LOG(2, "Process " << m_pid << " Recv wcs[" << i << "].slid = "<< wcs[i].slid);
                LOG(2, "Process " << m_pid << " Recv wcs[" << i << "].wr_id = "<< wcs[i].wr_id);
                LOG(2, "Process " << m_pid << " Recv wcs[" << i << "].imm_data = "<< wcs[i].imm_data);

                /**
                 * Here is a trick:
                 * The sender sends relatively generic LPF memslot ID.
                 * But for IB Verbs, we need to translate that into
                 * an IB Verbs slot via @getVerbID -- or there will be
                 * a mismatch when IB Verbs looks up the slot ID
                 */

                // Note: Ignore compare-and-swap atomics!
                if (wcs[i].opcode != IBV_WC_COMP_SWAP) {
                    SlotID slot;
                    // This receive is from a PUT call
                    if (wcs[i].opcode == IBV_WC_RECV_RDMA_WITH_IMM) {
                        slot = wcs[i].imm_data;
                        tryIncrement(Op::RECV, Phase::POST, slot);
                        LOG(3, "Rank " << m_pid << " increments received message count to " << rcvdMsgCount[slot] << " for LPF slot " << slot);
                    }
                }
                ibv_post_srq_recv(m_srq.get(), &wr, &bad_wr);
            }
        }
		if(pollResult > 0) totalResults += pollResult;
	} while (pollResult == POLL_BATCH && totalResults < MAX_POLLING);
}

void IBVerbs :: reconnectQPs()
{
    ASSERT( m_stagedQps[0] );
    m_comm.barrier();

    union ibv_gid myGid;
    std::vector< uint32_t> localQpNums, remoteQpNums;
    std::vector< uint16_t> lids;
    std::vector< union ibv_gid > gids;
    try {
        // Exchange info about the queue pairs
        if (m_gidIdx >= 0) {
            if (ibv_query_gid(m_device.get(), m_ibPort, m_gidIdx, &myGid)) {
                LOG(1, "Could not get GID of Infiniband device port " << m_ibPort);
                throw Exception( "Could not get gid for IB port");
            }
            LOG(3, "GID of Infiniband device was retrieved" );
        }
        else {
            std::memset( &myGid, 0, sizeof(myGid) );
            LOG(3, "GID of Infiniband device will not be used" );
        }

        localQpNums.resize(m_nprocs);
        remoteQpNums.resize(m_nprocs);
        lids.resize(m_nprocs);
        gids.resize(m_nprocs);

        for ( int i = 0; i < m_nprocs; ++i)
            localQpNums[i] = m_stagedQps[i]->qp_num;
    }
    catch(...)
    {
        m_comm.allreduceOr( true );
        throw;
    }
    if (m_comm.allreduceOr( false) )
        throw Exception("Peer failed to allocate memory or query device while setting-up QP");

    m_comm.allToAll( localQpNums.data(), remoteQpNums.data() );
    m_comm.allgather( m_lid, lids.data() );
    m_comm.allgather( myGid, gids.data() );

    LOG(3, "Connection initialisation data has been exchanged");

    try {
        // Bring QPs to INIT
        for (int i = 0; i < m_nprocs; ++i ) {
            struct ibv_qp_attr  attr;
            int                 flags;

            std::memset(&attr, 0, sizeof(attr));
            attr.qp_state = IBV_QPS_INIT;
            attr.port_num = m_ibPort;
            attr.pkey_index = 0;
            attr.qp_access_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_ATOMIC;
            flags = IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS;
            if ( ibv_modify_qp(m_stagedQps[i].get(), &attr, flags) ) {
                LOG(1, "Cannot bring state of QP " << i << " to INIT");
                throw Exception("Failed to bring QP's state to Init" );
            }

            // post a dummy receive

            struct ibv_recv_wr rr;  std::memset(&rr, 0, sizeof(rr));
            struct ibv_sge     sge; std::memset(&sge, 0, sizeof(sge));
            sge.addr = reinterpret_cast<uintptr_t>(m_dummyBuffer.data());
            sge.length = m_dummyBuffer.size();
            sge.lkey = m_dummyMemReg->lkey;
            rr.next = NULL;
            rr.wr_id = 46;
            rr.sg_list = &sge;
            rr.num_sge = 1;

            // Bring QP to RTR
            std::memset(&attr, 0, sizeof(attr));
            attr.qp_state = IBV_QPS_RTR;
            attr.path_mtu = m_mtu;
            attr.dest_qp_num = remoteQpNums[i];
            attr.rq_psn = 0;
            attr.max_dest_rd_atomic = 1;
            attr.min_rnr_timer = 0x12;
            attr.ah_attr.is_global = 0;
            attr.ah_attr.dlid = lids[i];
            attr.ah_attr.sl = 0;
            attr.ah_attr.src_path_bits  = 0;
            attr.ah_attr.port_num = m_ibPort;
            if (m_gidIdx >= 0)
            {
                attr.ah_attr.is_global = 1;
                attr.ah_attr.port_num = 1;
                memcpy(&attr.ah_attr.grh.dgid, &gids[i], 16);
                attr.ah_attr.grh.flow_label = 0;
                attr.ah_attr.grh.hop_limit = 1;
                attr.ah_attr.grh.sgid_index = m_gidIdx;
                attr.ah_attr.grh.traffic_class = 0;
            }
            flags = IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN | IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER;

            if (ibv_modify_qp(m_stagedQps[i].get(), &attr, flags)) {
                LOG(1, "Cannot bring state of QP " << i << " to RTR" );
                throw Exception("Failed to bring QP's state to RTR" );
            }

            // Bring QP to RTS
            std::memset(&attr, 0, sizeof(attr));
            attr.qp_state      = IBV_QPS_RTS;
            attr.timeout       = 0x12;
            attr.retry_cnt     = 0;//7;
            attr.rnr_retry     = 0;//7;
            attr.sq_psn        = 0;
            attr.max_rd_atomic = 1;
            flags = IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT |
                IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC;
            if( ibv_modify_qp(m_stagedQps[i].get(), &attr, flags))  {
                LOG(1, "Cannot bring state of QP " << i << " to RTS" );
                throw Exception("Failed to bring QP's state to RTS" );
            }

            LOG(3, "Connected Queue pair for " << m_pid << " -> " << i );

        } // for each peer
    }
            catch(...) {
                m_comm.allreduceOr( true );
                throw;
            }

            if (m_comm.allreduceOr( false ))
                throw Exception("Another peer failed to set-up Infiniband queue pairs");

            LOG(3, "All staged queue pairs have been connected" );

            m_connectedQps.swap( m_stagedQps );

            LOG(3, "All old queue pairs have been removed");

            m_comm.barrier();
        }


void IBVerbs :: resizeMemreg( size_t size )
{
    if ( size > size_t(std::numeric_limits<int>::max()) )
    {
        LOG(2, "Could not expand memory register, because integer will overflow");
        throw Exception("Could not increase memory register");
    }
    if ( int(size) > m_deviceAttr.max_mr ) {
        LOG(2, "IB device only supports " << m_deviceAttr.max_mr
                << " memory registrations, while " << size
                << " are being requested" );
        throw std::bad_alloc() ;
    }

    MemoryRegistration null = { 0, 0, 0, 0 };
    MemorySlot dflt; dflt.glob.resize( m_nprocs, null );

    m_memreg.reserve( size, dflt );
}

void IBVerbs :: resizeMesgq( size_t size )
{

    m_cqSize = std::min<size_t>(size,m_maxSrs/4);
	size_t remote_size = std::min<size_t>(m_cqSize*m_nprocs,m_maxSrs/4);
	if (m_cqLocal) {
		ibv_resize_cq(m_cqLocal.get(), m_cqSize);
	}
	if(remote_size >= m_postCount){
		if (m_cqRemote) {
			ibv_resize_cq(m_cqRemote.get(),  remote_size);
		}
	}
	stageQPs(m_cqSize);
    reconnectQPs();
	if(remote_size >= m_postCount){
		if (m_srq) {
			struct ibv_recv_wr wr;
			struct ibv_sge sg;
			struct ibv_recv_wr *bad_wr;
			sg.addr = (uint64_t) NULL;
			sg.length = 0;
			sg.lkey = 0;
			wr.next = NULL;
			wr.sg_list = &sg;
			wr.num_sge = 0;
			wr.wr_id = m_pid;
			for(int i = m_postCount; i < (int)remote_size; ++i){
				ibv_post_srq_recv(m_srq.get(), &wr, &bad_wr);
				m_postCount++;
			}
		}
	}
    LOG(4, "Message queue has been reallocated to size " << size );
}

IBVerbs :: SlotID IBVerbs :: regLocal( void * addr, size_t size )
{
    ASSERT( size <= m_maxRegSize );

    MemorySlot slot;
    if ( size > 0) {
        LOG(4, "Registering locally memory area at " << addr << " of size  " << size );
        struct ibv_mr * const ibv_mr_new_p = ibv_reg_mr(
            m_pd.get(), addr, size,
            IBV_ACCESS_REMOTE_READ | IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_ATOMIC
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
    MemoryRegistration local;
    local._addr = (char *) addr;
    local._size = size;
    local._lkey = size?slot.mr->lkey:0;
    local._rkey = size?slot.mr->rkey:0;

    SlotID id =  m_memreg.addLocalReg( slot );
    tryIncrement(Op::SEND/* <- dummy for init */, Phase::INIT, id);

    m_memreg.update( id ).glob.resize( m_nprocs );
    m_memreg.update( id ).glob[m_pid] = local;
    LOG(4, "Memory area " << addr << " of size " << size << " has been locally registered. Slot = " << id );
    return id;
}

IBVerbs :: SlotID IBVerbs :: regGlobal( void * addr, size_t size )
{
    ASSERT( size <= m_maxRegSize );

    MemorySlot slot;
    if ( size > 0 ) {
        LOG(4, "Registering globally memory area at " << addr << " of size  " << size );
        struct ibv_mr * const ibv_mr_new_p = ibv_reg_mr(
            m_pd.get(), addr, size,
            IBV_ACCESS_REMOTE_READ | IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_ATOMIC
        );
        if( ibv_mr_new_p == NULL )
            slot.mr.reset();
        else
            slot.mr.reset( ibv_mr_new_p, ibv_dereg_mr );
        if (!slot.mr) {
            LOG(1, "Could not register memory area at "
                   << addr << " of size " << size << " with IB device");
            m_comm.allreduceAnd(true);
            throw Exception("Could not register memory area");
        }
    }
    if (m_comm.allreduceOr(false))
        throw Exception("Another process could not register memory area");

    SlotID id = m_memreg.addGlobalReg( slot );
    tryIncrement(Op::SEND/* <- dummy for init */, Phase::INIT, id);
    MemorySlot & ref = m_memreg.update(id);
    // exchange memory registration info globally
    ref.glob.resize(m_nprocs);

    MemoryRegistration local;
    local._addr = (char *) addr;
    local._size = size;
    local._lkey = size?slot.mr->lkey:0;
    local._rkey = size?slot.mr->rkey:0;

    LOG(4, "All-gathering memory register data" );

    m_comm.allgather( local, ref.glob.data() );
    LOG(4, "Memory area " << addr << " of size " << size << " has been globally registered. Slot = " << id );
    return id;
}

void IBVerbs :: dereg( SlotID id )
{
    slotActive[id] = false;
    m_recvInitMsgCount[id] = 0;
    m_getInitMsgCount[id] = 0;
    m_sendInitMsgCount[id] = 0;
    rcvdMsgCount[id] = 0;
    sentMsgCount[id] = 0;
    m_memreg.removeReg( id );
    LOG(4, "Memory area of slot " << id << " has been deregistered");
}


void IBVerbs :: blockingCompareAndSwap(SlotID srcSlot, size_t srcOffset, int dstPid, SlotID dstSlot, size_t dstOffset, size_t size, uint64_t compare_add, uint64_t swap)
{
	const MemorySlot & src = m_memreg.lookup( srcSlot );
	const MemorySlot & dst = m_memreg.lookup( dstSlot);

    char * localAddr
        = static_cast<char *>(src.glob[m_pid]._addr) + srcOffset;
        const char * remoteAddr
            = static_cast<const char *>(dst.glob[dstPid]._addr) + dstOffset;

	struct ibv_sge sge;
	memset(&sge, 0, sizeof(sge));
        sge.addr = reinterpret_cast<uintptr_t>( localAddr );
	sge.length =  std::min<size_t>(size, m_maxMsgSize );
        sge.lkey = src.mr->lkey;

	struct ibv_send_wr wr;
	memset(&wr, 0, sizeof(wr));
	wr.wr_id = srcSlot;
	wr.sg_list = &sge;
	wr.next = NULL; // this needs to be set, otherwise EINVAL return error in ibv_post_send
	wr.num_sge = 1;
	wr.opcode     = IBV_WR_ATOMIC_CMP_AND_SWP;
	wr.send_flags = IBV_SEND_SIGNALED;
	wr.wr.atomic.remote_addr = reinterpret_cast<uintptr_t>(remoteAddr);
	wr.wr.atomic.compare_add = compare_add;
	wr.wr.atomic.swap = swap;
	wr.wr.atomic.rkey = dst.glob[dstPid]._rkey;
	struct ibv_send_wr *bad_wr;
	int error;
    std::vector<ibv_wc_opcode> opcodes;

blockingCompareAndSwap:
	if (int err = ibv_post_send(m_connectedQps[dstPid].get(), &wr, &bad_wr ))
	{
		LOG(1, "Error while posting RDMA requests: " << std::strerror(err) );
		throw Exception("Error while posting RDMA requests");
	}

    /**
     * Keep waiting on a completion of events until you 
     * register a completed atomic compare-and-swap
     */
    do {
        opcodes = wait_completion(error);
         if (error) {
            LOG(1, "Error in wait_completion");
            std::abort();
        }
    } while (std::find(opcodes.begin(), opcodes.end(), IBV_WC_COMP_SWAP) == opcodes.end());

	uint64_t * remoteValueFound = reinterpret_cast<uint64_t *>(localAddr);
	/* 
     * if we fetched the value we expected, then
     * we are holding the lock now (that is, we swapped successfully!)
     * else, re-post your request for the lock
     */
	if (remoteValueFound[0] != compare_add)  {
        LOG(4, "Process " << m_pid <<  " couldn't get the lock. remoteValue = " << remoteValueFound[0] << " compare_add = " << compare_add  << " go on, iterate\n");
		goto blockingCompareAndSwap;
    }
    else {
        LOG(4, "Process " << m_pid << " reads value " << remoteValueFound[0] << " and expected = " << compare_add  <<" gets the lock, done\n");
    }
	// else we hold the lock and swap value into the remote slot ...
}

void IBVerbs :: put( SlotID srcSlot, size_t srcOffset,
              int dstPid, SlotID dstSlot, size_t dstOffset, size_t size)
{
    const MemorySlot & src = m_memreg.lookup( srcSlot );
    const MemorySlot & dst = m_memreg.lookup( dstSlot );

    ASSERT( src.mr );
    ASSERT( dst.mr );

    int numMsgs = size/m_maxMsgSize + (size % m_maxMsgSize > 0); //+1 if last msg size < m_maxMsgSize
    if (size == 0) numMsgs = 1;

    struct ibv_sge	   sges[numMsgs];
    struct ibv_send_wr srs[numMsgs];
    struct ibv_sge	   *sge;
    struct ibv_send_wr *sr;
    for (int i=0; i < numMsgs; i++) {
        sge = &sges[i]; std::memset(sge, 0, sizeof(ibv_sge));
        sr = &srs[i]; std::memset(sr, 0, sizeof(ibv_send_wr));
        const char * localAddr
            = static_cast<const char *>(src.glob[m_pid]._addr) + srcOffset;
        const char * remoteAddr
            = static_cast<const char *>(dst.glob[dstPid]._addr) + dstOffset;

        sge->addr = reinterpret_cast<uintptr_t>( localAddr );
        sge->length =  std::min<size_t>(size, m_maxMsgSize );
        sge->lkey = src.mr->lkey;
        sges[i] = *sge;

        bool lastMsg = (i == numMsgs-1);
        sr->next = lastMsg ? NULL : &srs[ i+1];
        // since reliable connection guarantees keeps packets in order,
        // we only need a signal from the last message in the queue
        sr->send_flags = lastMsg ? IBV_SEND_SIGNALED : 0;
        sr->opcode = lastMsg? IBV_WR_RDMA_WRITE_WITH_IMM : IBV_WR_RDMA_WRITE;
        /* use wr_id to later demultiplex srcSlot */
        sr->wr_id = srcSlot; 
        /*
         * In HiCR, we need to know at receiver end which slot 
         * has received the message. But here is a trick:
         */
        sr->imm_data = dstSlot;

        sr->sg_list = &sges[i];
        sr->num_sge = 1;
        sr->wr.rdma.remote_addr = reinterpret_cast<uintptr_t>( remoteAddr );
        sr->wr.rdma.rkey = dst.glob[dstPid]._rkey;

        srs[i] = *sr;
        size -= sge->length;
        srcOffset += sge->length;
        dstOffset += sge->length;

        LOG(4, "PID " << m_pid << ": Enqueued put message of " << sge->length << " bytes to " << dstPid << " on slot" << dstSlot );

    }
    struct ibv_send_wr *bad_wr = NULL;
    // srs[0] should be sufficient because the rest of srs are on a chain
    if (int err = ibv_post_send(m_connectedQps[dstPid].get(), &srs[0], &bad_wr ))
    {
        LOG(1, "Error while posting RDMA requests: " << std::strerror(err) );
        throw Exception("Error while posting RDMA requests");
    }

    tryIncrement(Op::SEND, Phase::PRE, srcSlot);
}

void IBVerbs :: get( int srcPid, SlotID srcSlot, size_t srcOffset,
              SlotID dstSlot, size_t dstOffset, size_t size )
{
    const MemorySlot & src = m_memreg.lookup( srcSlot );
	const MemorySlot & dst = m_memreg.lookup( dstSlot );

	ASSERT( dst.mr );

	int numMsgs = size/m_maxMsgSize + (size % m_maxMsgSize > 0); //+1 if last msg size < m_maxMsgSize

	struct ibv_sge	   sges[numMsgs+1];
	struct ibv_send_wr srs[numMsgs+1];
	struct ibv_sge	   *sge;
	struct ibv_send_wr *sr;


	for(int i = 0; i< numMsgs; i++){
		sge = &sges[i]; std::memset(sge, 0, sizeof(ibv_sge));
		sr = &srs[i]; std::memset(sr, 0, sizeof(ibv_send_wr));

		const char * localAddr
			= static_cast<const char *>(dst.glob[m_pid]._addr) + dstOffset;
		const char * remoteAddr
			= static_cast<const char *>(src.glob[srcPid]._addr) + srcOffset;

		sge->addr = reinterpret_cast<uintptr_t>( localAddr );
		sge->length = std::min<size_t>(size, m_maxMsgSize );
		sge->lkey = dst.mr->lkey;
        sges[i] = *sge;
        LOG(4, "PID " << m_pid << ": Enqueued get message of " << sge->length << " bytes from " << srcPid << " on slot" << srcSlot );

        bool lastMsg = (i == numMsgs-1);
        sr->next = lastMsg ? NULL : &srs[ i+1];
		sr->send_flags = lastMsg ? IBV_SEND_SIGNALED : 0;

		sr->sg_list = &sges[i];
		sr->num_sge = 1;
		sr->opcode = IBV_WR_RDMA_READ;
		sr->wr.rdma.remote_addr = reinterpret_cast<uintptr_t>( remoteAddr );
		sr->wr.rdma.rkey = src.glob[srcPid]._rkey;
        // This logic is reversed compared to ::put
        // (not srcSlot, as this slot is remote)
        sr->wr_id = dstSlot; // <= DO NOT CHANGE THIS !!!
        sr->imm_data = srcSlot; // This is irrelevant as we don't send _WITH_IMM
        srs[i] = *sr;
		size -= sge->length;
		srcOffset += sge->length;
		dstOffset += sge->length;
	}

	struct ibv_send_wr *bad_wr = NULL;
	if (int err = ibv_post_send(m_connectedQps[srcPid].get(), &srs[0], &bad_wr ))
	{

		LOG(1, "Error while posting RDMA requests: " << std::strerror(err) );
        if (err == ENOMEM) {
            LOG(1, "Specific error code: ENOMEM (send queue is full or no resources)");
        }
		throw Exception("Error while posting RDMA requests");
	}
    tryIncrement(Op::GET, Phase::PRE, dstSlot);

}

void IBVerbs :: get_rcvd_msg_count(size_t * rcvd_msgs) {
    *rcvd_msgs = m_recvdMsgs;
}

void IBVerbs :: get_sent_msg_count(size_t * sent_msgs) {
    *sent_msgs = m_sentMsgs;
}

void IBVerbs :: get_rcvd_msg_count_per_slot(size_t * rcvd_msgs, SlotID slot)
{
    *rcvd_msgs = rcvdMsgCount[slot];
}

void IBVerbs :: get_sent_msg_count_per_slot(size_t * sent_msgs, SlotID slot)
{
    *sent_msgs = sentMsgCount.at(slot);
}

std::vector<ibv_wc_opcode> IBVerbs :: wait_completion(int& error) {

    error = 0;
    LOG(5, "Polling for messages" );
    struct ibv_wc wcs[POLL_BATCH];
    int pollResult = ibv_poll_cq(m_cqLocal.get(), POLL_BATCH, wcs);
    std::vector<ibv_wc_opcode> opcodes;
    if ( pollResult > 0) {
        LOG(3, "Process " << m_pid << ": Received " << pollResult << " acknowledgements");

        for (int i = 0; i < pollResult ; ++i) {
            if (wcs[i].status != IBV_WC_SUCCESS)
            {
                LOG( 2, "Got bad completion status from IB message."
                        " status = 0x" << std::hex << wcs[i].status
                        << ", vendor syndrome = 0x" << std::hex
                        << wcs[i].vendor_err );
                const char * status_descr;
                status_descr = ibv_wc_status_str(wcs[i].status);
                LOG( 2, "The work completion status string: " << status_descr);
                error = 1;
            }
            else {
                LOG(3, "Process " << m_pid << " Send wcs[" << i << "].src_qp = "<< wcs[i].src_qp);
                LOG(3, "Process " << m_pid << " Send wcs[" << i << "].slid = "<< wcs[i].slid);
                LOG(3, "Process " << m_pid << " Send wcs[" << i << "].wr_id = "<< wcs[i].wr_id);
                LOG(3, "Process " << m_pid << " Send wcs[" << i << "].imm_data = "<< wcs[i].imm_data);
            }

            SlotID slot = wcs[i].wr_id;
            opcodes.push_back(wcs[i].opcode);
            // Ignore compare-and-swap atomics!
            if (wcs[i].opcode != IBV_WC_COMP_SWAP) {
                // This is a get call completing
                if (wcs[i].opcode == IBV_WC_RDMA_READ) {
                    tryIncrement(Op::GET, Phase::POST, slot);
                }
                // This is a put call completing
                if (wcs[i].opcode == IBV_WC_RDMA_WRITE)
                    tryIncrement(Op::SEND, Phase::POST, slot);

                LOG(3, "Rank " << m_pid << " increments sent message count to " << sentMsgCount[slot] << " for LPF slot " << slot);
            }
        }
    }
    else if (pollResult < 0)
    {
        LOG( 5, "Failed to poll IB completion queue" );
        throw Exception("Poll CQ failure");
    }
    return opcodes;
}

void IBVerbs :: flushReceived() {
        doRemoteProgress();
}

void IBVerbs :: flushSent()
{
    int isError = 0;

    bool sendsComplete;
    do {
        sendsComplete = true;
        for (size_t i = 0; i<ARRAY_SIZE; i++) {
            if (slotActive[i]) {
                if (m_sendInitMsgCount[i] > sentMsgCount[i] || m_getInitMsgCount[i] > getMsgCount[i]) {
                    sendsComplete = false;
                    wait_completion(isError);
                    if (isError) {
                        LOG(1, "Error in wait_completion. Most likely issue is that receiver is not calling ibv_post_srq!\n");
                        std::abort();
                    }
                }
            }
        }
    } while (!sendsComplete);

}

void IBVerbs :: countingSyncPerSlot(SlotID slot, size_t expectedSent, size_t expectedRecvd) {

    int error;
    if (slotActive[slot]) {
        do {
            wait_completion(error);
            if (error) {
                LOG(1, "Error in wait_completion");
                std::abort();
            }
            // this call triggers doRemoteProgress
            doRemoteProgress();

        } while ((
                // do we have messages (sent or received)
                // which are only initiated but incomplete?
                (rcvdMsgCount[slot] < m_recvInitMsgCount[slot]) ||
                (sentMsgCount[slot] < m_sendInitMsgCount[slot])
                ) && 
            // do the sent and received messages
            // match our expectations?
            (rcvdMsgCount[slot] < expectedRecvd
            || sentMsgCount[slot] < expectedSent));
    }
}

void IBVerbs :: syncPerSlot(SlotID slot) {
    int error;

    do {
        wait_completion(error);
        if (error) {
            LOG(1, "Error in wait_completion");
            std::abort();
        }
        doRemoteProgress();
    }
    while ((rcvdMsgCount.at(slot) < m_recvInitMsgCount.at(slot)) || (sentMsgCount.at(slot) < m_sendInitMsgCount.at(slot)));

    /**
     * A subsequent barrier is a controversial decision:
     * - if we use it, the sync guarantees that
     *   receiver has received all that it is supposed to
     *   receive. However, it loses all performance advantages
     *   of waiting "only on certain tags"
     * - if we do not barrier, we only make sure the slot
     *   completes all sends and receives that HAVE ALREADY
     *   BEEN ISSUED. However, a receiver of an RMA put
     *   cannot know if it is supposed to receive more messages.
     *   It can only know if it is receiving via an RMA get.
     *   Therefore, now this operation is commented
    */
    //m_comm.barrier();

}

void IBVerbs :: sync(bool resized)
{
    (void) resized;

    // flush send queues
    flushSent();
    // flush receive queues
    flushReceived();

    LOG(1, "Process " << m_pid << " will call barrier\n");
    m_comm.barrier();


}


} }
