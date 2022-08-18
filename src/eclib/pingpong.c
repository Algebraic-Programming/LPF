/**
 * Copyright (c) 2013-2015 Intel Corporation. All rights reserved.
 * Copyright (c) 2014-2016, Cisco Systems, Inc. All rights reserved.
 * Copyrigth (c) 2015 Los Alamos Nat. Security, LLC. All rights resreved.
 * Copyrifht (c) 2016 Cray Inc. All rights reserved.
 * 
 * This software is available to you under the BSD license below:
 * 
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 * 
 *      - Redistributions of source code must retain the above
 *        notice, this list of conditions and the following
 *        disclaimer.
 * 
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT ANY WARRANTY OF ANY KIND,
 * EXPRESS OF IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OF COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * 	Copyright (c) 2022 Huawei Technologies Co., Ltd.
 * 
 * Licenced under the Apache License, Version 2.0 (the "Licence");
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * 		http://www.apache.org/licences/Licence-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITOUT WARRANTIES OR CONDITIONS OF ANY KINF, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <assert.h>
#include <getopt.h>
#include <inttypes.h>
#include <netdb.h>
#include <poll.h>
#include <limits.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>

#include <rdma/fabric.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_eq.h>
#include <rdma/fi_errno.h>
#include <rdma/fi_tagged.h>
#include "pingpong.h"
#include <mpi.h>
#include "ec.h"
#include <math.h>

const uint64_t TAG = 1234;

int pp_debug;
int pp_ipv6;

void pp_start(struct ec_ct *ct)
{
	PP_DEBUG("Starting test chrono\n");
	ct->start = pp_gettime_us();
}

void pp_stop(struct ec_ct *ct)
{
	ct->end = pp_gettime_us();
	PP_DEBUG("Stopped test chrono\n");
}

size_t ec_packet_size(struct ec_ct *ct){
	return (size_t) ct->fi->tx_attr->inject_size - sizeof(struct ec_packet);
}

uint64_t pp_gettime_us(void)
{
	struct timeval now;

	gettimeofday(&now, NULL);
	return now.tv_sec * 1000000 + now.tv_usec;
}

int ec_send_recv_name(struct ec_ct *ct, struct fid *endpoint, int src_rank, int dest_rank, MPI_Comm comm)
{
	int myrank = -1;
	int ret = MPI_Comm_rank(comm, &myrank);
	if(ret) return ret;
	MPI_Status status;
	char addr_format_buf[sizeof(ct->hints[myrank]->addr_format)];
	uint32_t len;
	uint32_t lenbuf;

	if(!ct->local_name){

		ct->local_name = calloc(1, ct->addrlen);
		if (!ct->local_name) {
			PP_ERR("Failed to allocate memory for the address\n");
			return -ENOMEM;
		}

		ret = fi_getname(endpoint, ct->local_name, &ct->addrlen);
		if (ret) {
			PP_PRINTERR("fi_getname", ret);
			return ret;
		}
	}

	if(myrank == src_rank && myrank == dest_rank){
		memcpy(ct->rem_name.rem_name_ptr[myrank], ct->local_name, ct->addrlen);
		return 0;	
	}

	PP_DEBUG("Sending and receiving name length\n");
	len = ct->addrlen;
	ret = MPI_Sendrecv((void*) &len, sizeof(uint32_t),
						MPI_BYTE, dest_rank, TAG,
						&lenbuf, sizeof(uint32_t), MPI_BYTE, 
						src_rank, TAG, comm, 
						&status);
	len = lenbuf;
	if (ret != 0)
		return ret;

	if (len > PP_MAX_ADDRLEN)
		return -EINVAL;

	PP_DEBUG("Sending and receiving address format\n");
	ret = MPI_Sendrecv((void*) &(ct->fi->addr_format), 
						sizeof(ct->fi->addr_format), 
						MPI_BYTE, dest_rank, TAG,
						addr_format_buf, sizeof(ct->hints[src_rank]->addr_format), 
						MPI_BYTE, src_rank, 
						TAG, comm, &status);

	ct->hints[src_rank]->addr_format = *((uint32_t*) addr_format_buf);
	if (ret != 0)
		return ret;

	PP_DEBUG("Sending and receiving name\n");
	ret = MPI_Sendrecv((void*) ct->local_name, ct->addrlen, 
						MPI_BYTE, dest_rank, 
						TAG, ct->rem_name.rem_name_ptr[src_rank], len, MPI_BYTE, 
						src_rank, TAG, 
						comm, &status);

	if (ret != 0)
		return ret;
	PP_DEBUG("Received name\n");

	ct->hints[src_rank]->dest_addr = calloc(1, len);
	if (!ct->hints[src_rank]->dest_addr) {
		PP_DEBUG("Failed to allocate memory for destination address\n");
		return -ENOMEM;
	}

	/* fi_freeinfo will free the dest_addr field. */
	memcpy(ct->hints[src_rank]->dest_addr, ct->rem_name.rem_name_ptr[src_rank], len);
	ct->hints[src_rank]->dest_addrlen = len;
	return 0;
}

/*******************************************************************************
 *                                      Data Messaging
 ******************************************************************************/

int pp_cq_readerr(struct fid_cq *cq)
{
	struct fi_cq_err_entry cq_err = { 0 };
	int ret;

	ret = fi_cq_readerr(cq, &cq_err, 0);
	if (ret < 0) {
		PP_PRINTERR("fi_cq_readerr", ret);
	} else {
		PP_ERR("cq_readerr: %s",
		       fi_cq_strerror(cq, cq_err.prov_errno, cq_err.err_data,
				      NULL, 0));
		ret = -cq_err.err;
	}
	return ret;
}

int pp_get_cq_comp(struct fid_cq *cq, uint64_t *cur, uint64_t total, int timeout_usec)
{
	struct fi_cq_err_entry comp;
	uint64_t a = 0, b = 0;
	int ret = 0;

	if (timeout_usec >= 0)
		a = pp_gettime_us();  
 
	do {
		ret = fi_cq_read(cq, &comp, 1);
		if (ret > 0) {
			if (timeout_usec >= 0)
				a = pp_gettime_us();

			(*cur)++;
		} else if (ret < 0 && ret != -FI_EAGAIN) {
			if (ret == -FI_EAVAIL) {
				ret = pp_cq_readerr(cq);
				(*cur)++;
			} else {
				PP_PRINTERR("pp_get_cq_comp", ret);
			}

			return ret;
		} else if (timeout_usec >= 0) {
			b = pp_gettime_us();
			if ((b - a)  > (uint64_t) timeout_usec) {
				fprintf(stderr, "%dus timeout expired\n",
					timeout_usec);
				return -FI_ENODATA;
			}
		}
	} while (total - *cur > 0);

	return 0;
}

int pp_get_rx_comp(struct ec_ct *ct, uint64_t total)
{
	int ret = FI_SUCCESS;

	if (ct->rx.cq) {
		ret = pp_get_cq_comp(ct->rx.cq, &(ct->rx.cq_cntr), total,
				     ct->timeout_usec);
	} else {
		PP_ERR(
		    "Trying to get a RX completion when no RX CQ was opened");
		ret = -FI_EOTHER;
	}
	return ret;
}

int pp_get_tx_comp(struct ec_ct *ct, uint64_t total)
{
	int ret;

	if (ct->tx.cq) {
		ret = pp_get_cq_comp(ct->tx.cq, &(ct->tx.cq_cntr), total, -1);
	} else {
		PP_ERR(
		    "Trying to get a TX completion when no TX CQ was opened");
		ret = -FI_EOTHER;
	}
	return ret;
}	 
           

ssize_t ec_tx(struct ec_ct *ct, struct fid_ep *ep, size_t size, int dest_rank){

	int ret, rc;
	while (1) {
		ret = fi_inject(ep,ct->tx.buf, size, ct->remote_fi_addr[dest_rank]);
		if (!ret)
			break;
		if (ret != -FI_EAGAIN) {
			PP_PRINTERR("inject", ret);
			return ret;
		}
		rc = pp_get_tx_comp(ct, ct->tx.seq);
		
		if (rc && rc != -FI_EAGAIN) {
			PP_ERR("Failed to get inject completion");
			return rc;
		}
	}
	ct->tx.seq++; 
	ct->tx.cq_cntr++;

	return 0;
}

ssize_t ec_rx(struct ec_ct *ct)
{
	ssize_t ret;

	ret = pp_get_rx_comp(ct, ct->rx.seq);
	if (ret)
		return ret;

	/* TODO: verify CQ data, if available */

	/* Ignore the size arg. Post a buffer large enough to handle all message
	 * sizes. pp_sync() makes use of ec_rx() and gets called in tests just
	 * before message size is updated. The recvs posted are always for the
	 * next incoming message.
	 */

	int rc;
	while (1) {
		ret = (int) fi_recv(ct->ep, ct->rx.buf, MAX(ct->rx.size , (size_t) PP_MAX_CTRL_MSG) + ct->rx.prefix_size, NULL, 0, ct->rx.ctx_ptr);
		if (!ret)
			break;


		if (ret != -FI_EAGAIN) {
			PP_PRINTERR("receive", ret);
			return ret;
		}
		rc = pp_get_rx_comp(ct, ct->rx.seq);
		if (rc && rc != -FI_EAGAIN) {
			PP_ERR("Failed to get receive completion");
			return rc;
		}
	}
	ct->rx.seq++;

	ct->cnt_ack_msg++;

	return ret;
}

/*******************************************************************************
 *                                Initialization and allocations
 ******************************************************************************/

uint64_t pp_init_cq_data(struct fi_info *info)
{
	if (info->domain_attr->cq_data_size >= sizeof(uint64_t)) {
		return 0x0123456789abcdefULL;
	} else {
		return 0x0123456789abcdefULL &
		       ((0x1ULL << (info->domain_attr->cq_data_size * 8)) - 1);
	}
}

int pp_alloc_msgs(struct ec_ct *ct)
{
	int ret;
	long alignment = getpagesize();

	ct->tx.size = PP_MAX_DATA_MSG;
	if (ct->tx.size > ct->fi->ep_attr->max_msg_size)
		ct->tx.size = ct->fi->ep_attr->max_msg_size;
	ct->rx.size = ct->tx.size;
	ct->buf_size = MAX(ct->tx.size, (size_t) PP_MAX_CTRL_MSG) +
		       MAX(ct->rx.size, (size_t) PP_MAX_CTRL_MSG) +
		       ct->tx.prefix_size + ct->rx.prefix_size;

	if (alignment < 0) {
		PP_PRINTERR("ofi_get_page_size", alignment);
		abort();
	}
	/* Extra alignment for the second part of the buffer */
	ct->buf_size += alignment;

	ret = posix_memalign(&(ct->buf), (size_t)alignment, ct->buf_size);
	if (ret) {
		PP_PRINTERR("ofi_memalign", ret);
		return ret;
	}
	memset(ct->buf, 0, ct->buf_size);
	ct->rx.buf = ct->buf;
	ct->tx.buf = (char *)ct->buf +
			MAX(ct->rx.size, (size_t) PP_MAX_CTRL_MSG) +
			ct->tx.prefix_size;
	ct->tx.buf = (void *)(((uintptr_t)ct->tx.buf + alignment - 1) &
			      ~(alignment - 1));

	ct->remote_cq_data = pp_init_cq_data(ct->fi);

	return 0;
}

int pp_open_fabric_res(struct ec_ct *ct)
{
	int ret;

	PP_DEBUG("Opening fabric resources: fabric, eq & domain\n");

	ret = fi_fabric(ct->fi->fabric_attr, &(ct->fabric), NULL);
	if (ret) {
		PP_PRINTERR("fi_fabric", ret);
		return ret;
	}
	ret = fi_eq_open(ct->fabric, &(ct->eq_attr), &(ct->eq), NULL);
	if (ret) {
		PP_PRINTERR("fi_eq_open", ret);
		return ret;
	}

	ret = fi_domain(ct->fabric, ct->fi, &(ct->domain), NULL);
	if (ret) {
		PP_PRINTERR("fi_domain", ret);
		return ret;
	}

	PP_DEBUG("Fabric resources opened\n");

	return 0;
}

int pp_alloc_active_res(struct ec_ct *ct, struct fi_info *fi, MPI_Comm comm)
{
	int nprocs = -1;
	int ret = MPI_Comm_size(comm, &nprocs);
	if(ret) return ret;
	if (fi->tx_attr->mode & FI_MSG_PREFIX)
		ct->tx.prefix_size = fi->ep_attr->msg_prefix_size;
	if (fi->rx_attr->mode & FI_MSG_PREFIX)
		ct->rx.prefix_size = fi->ep_attr->msg_prefix_size;

	ret = pp_alloc_msgs(ct);
	if (ret)
		return ret;

	if (ct->cq_attr.format == FI_CQ_FORMAT_UNSPEC)
		ct->cq_attr.format = FI_CQ_FORMAT_CONTEXT;

	ct->cq_attr.wait_obj = FI_WAIT_NONE;

	ct->cq_attr.size = fi->tx_attr->size;
	ret = fi_cq_open(ct->domain, &(ct->cq_attr), &(ct->tx.cq), &(ct->tx.cq));
	if (ret) {
		PP_PRINTERR("fi_cq_open", ret);
		return ret;
	}
	
	ct->cq_attr.size = fi->rx_attr->size;
	ret = fi_cq_open(ct->domain, &(ct->cq_attr), &(ct->rx.cq), &(ct->rx.cq));
	if (ret) {
		PP_PRINTERR("fi_cq_open", ret);
		return ret;
	}

	
	if (fi->domain_attr->av_type != FI_AV_UNSPEC)
		ct->av_attr.type = fi->domain_attr->av_type;

	ct->av_attr.type = FI_AV_TABLE;
	ct->av_attr.count = nprocs;
	ret = fi_av_open(ct->domain, &(ct->av_attr), &(ct->av), NULL);
	if (ret) {
		PP_PRINTERR("fi_av_open", ret);
		return ret;
	}


	ret = fi_endpoint(ct->domain, fi, &(ct->ep), NULL);
	if (ret) {
		PP_PRINTERR("fi_endpoint", ret);
		return ret;
	}

	return 0;
}

 int pp_getinfo(struct ec_ct *ct, struct fi_info *hints,
		      struct fi_info **info)
{
	uint64_t flags = 0;
	int ret = 0;

	if (!hints->ep_attr->type)
		hints->ep_attr->type = FI_EP_DGRAM;
	ret = fi_getinfo(FI_VERSION(FI_MAJOR_VERSION, FI_MINOR_VERSION),
			 NULL, NULL, flags, hints, info);
	if (ret) {
		PP_PRINTERR("fi_getinfo", ret);
		return ret;
	}
#if defined FI_CONTEXT2
	if (((*info)->tx_attr->mode & FI_CONTEXT2) != 0) {
		ct->tx.ctx_ptr = &(ct->tx.ctx[0]);
	} else
#endif
           if (((*info)->tx_attr->mode & FI_CONTEXT) != 0) {
		ct->tx.ctx_ptr = &(ct->tx.ctx[1]);
	} else 
#if defined FI_CONTEXT2
			if (((*info)->mode & FI_CONTEXT2) != 0) {
		ct->tx.ctx_ptr = &(ct->tx.ctx[0]);
	} else 
#endif
			if (((*info)->mode & FI_CONTEXT) != 0) {
		ct->tx.ctx_ptr = &(ct->tx.ctx[1]);
	} else {
		ct->tx.ctx_ptr = NULL;
	}
#if defined FI_CONTEXT2
	if (((*info)->rx_attr->mode & FI_CONTEXT2) != 0) {
		ct->rx.ctx_ptr = &(ct->rx.ctx[0]);
	} else 
#endif
			if (((*info)->rx_attr->mode & FI_CONTEXT) != 0) {
		ct->rx.ctx_ptr = &(ct->rx.ctx[1]);
	} else 
#if defined FI_CONTEXT2
			if (((*info)->mode & FI_CONTEXT2) != 0) {
		ct->rx.ctx_ptr = &(ct->rx.ctx[0]);
	} else 
#endif
			if (((*info)->mode & FI_CONTEXT) != 0) {
		ct->rx.ctx_ptr = &(ct->rx.ctx[1]);
	} else {
		ct->rx.ctx_ptr = NULL;
	}

	return 0;
}

#define PP_EP_BIND(ep, fd, flags)                                              \
	do {                                                                   \
		int ret;                                                       \
		if ((fd)) {                                                    \
			ret = fi_ep_bind((ep), &(fd)->fid, (flags));           \
			if (ret) {                                             \
				PP_PRINTERR("fi_ep_bind", ret);                \
				return ret;                                    \
			}                                                      \
		}                                                              \
	} while (0)

int pp_init_ep(struct ec_ct *ct)
{
	int ret;

	PP_DEBUG("Initializing endpoint\n");
	PP_EP_BIND(ct->ep, ct->av, 0);
	PP_EP_BIND(ct->ep, ct->tx.cq, FI_TRANSMIT);
	PP_EP_BIND(ct->ep, ct->rx.cq, FI_RECV);

	ret = fi_enable(ct->ep);
	if (ret) {
		PP_PRINTERR("fi_enable", ret);
		return ret;
	}

	// int timeout_usec_save;
	int rc;
	while (1) {
		ret = (int) fi_recv(ct->ep, ct->rx.buf, MAX(ct->rx.size , (size_t) PP_MAX_CTRL_MSG) + ct->rx.prefix_size, NULL, 0, ct->rx.ctx_ptr);
		if (!ret)
			break;

		if (ret != -FI_EAGAIN) {
			PP_PRINTERR("receive", ret);
			return ret;
		}

		// timeout_usec_save = ct->timeout_usec;
		// ct->timeout_usec = 0;
		rc = pp_get_rx_comp(ct, ct->rx.seq);
		// ct->timeout_usec = timeout_usec_save;
		if (rc && rc != -FI_EAGAIN) {
			PP_ERR("Failed to get receive completion");
			return rc;
		}
	}
	ct->rx.seq++;	
	if (ret)
		return ret;

	PP_DEBUG("Endpoint initialized\n");

	return 0;
}

int pp_av_insert(struct fid_av *av, void *addr, size_t count,
			fi_addr_t *fi_addr, uint64_t flags, void *context)
{
	int ret;

	PP_DEBUG("Connection-less endpoint: inserting new address in vector\n");

	ret = fi_av_insert(av, addr, count, fi_addr, flags, context);
	if (ret < 0) {
		PP_PRINTERR("fi_av_insert", ret);
		return ret;
	} else if ((size_t) ret != count) {
		PP_ERR("fi_av_insert: number of addresses inserted = %d;"
		       " number of addresses given = %zd\n",
		       ret, count);
		return -EXIT_FAILURE;
	}

	PP_DEBUG("Connection-less endpoint: new address inserted in vector\n");

	return 0;
}

/*******************************************************************************
 *                                Deallocations and Final
 ******************************************************************************/

int pp_finalize(struct ec_ct *ct, MPI_Comm comm)
{
	struct iovec iov;
	int nprocs = -1;
	int myrank = -1;
	int ret = MPI_Comm_size(comm, &nprocs);
	if(ret) return ret;
	ret = MPI_Comm_rank(comm, &myrank);
	if(ret) return ret;
	int i = 0;
	struct fi_context ctx[2];
	const char *fin_buf = "fin";
	const size_t fin_buf_size = strlen(fin_buf) + 1;

	PP_DEBUG("Terminating test\n");

	snprintf(ct->tx.buf, fin_buf_size, "%s", fin_buf);

	iov.iov_base = ct->tx.buf;
	iov.iov_len = fin_buf_size + ct->tx.prefix_size;
	for(i=1; i<nprocs; i++){
		if (!(ct->fi->caps & FI_TAGGED)) {
			struct fi_msg msg = {
				.msg_iov = &iov,
				.iov_count = 1,
				.addr = ct->remote_fi_addr[(myrank + i)%nprocs],
				.context = ctx,
			};

			ret = fi_sendmsg(ct->ep, &msg, FI_TRANSMIT_COMPLETE);
			if (ret) {
				PP_PRINTERR("transmit", ret);
				return ret;
			}
		} else {
			struct fi_msg_tagged tmsg = {
				.msg_iov = &iov,
				.iov_count = 1,
				.addr = ct->remote_fi_addr[(myrank + i)%nprocs],
				.context = ctx,
				.tag = TAG,
			};

			ret = fi_tsendmsg(ct->ep, &tmsg, FI_TRANSMIT_COMPLETE);
			if (ret) {
				PP_PRINTERR("t-transmit", ret);
				return ret;
			}
		}
	}

	ret = pp_get_tx_comp(ct, ++ct->tx.seq);
	if (ret)
		return ret;
	ret = pp_get_rx_comp(ct, ct->rx.seq);
	if (ret)
		return ret;
	PP_DEBUG("Test terminated\n");

	return 0;
}