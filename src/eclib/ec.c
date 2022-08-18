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

#include <stdio.h>
#include <mpi.h>
#include "pingpong.h"
#include "perfs.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <rdma/fabric.h>

/**
 * @brief Clear the buffer of the given ec_c_info structure
 * 
 * @param[out] X The ec_x_info instance needing its buffer cleared 
 */
void clear_bufx(struct ec_x_info X){
	memset(X.buf, 0, X.size + X.prefix_size);
}

/**
 * @brief "Destruction" of the context (unallocation of the memory and reset of the values)
 * 
 * This method will free all the memory used by the given context and close every fabric
 * identifier related to it.
 * 
 * WARNING: After using this function, the context cannot be initialized using ec_create_ct
 * without iniatilizing every other processes with it.
 * 
 * @param[in] ct The context to erase and unallocate
 * @param[in] comm The relevant MPI Communicator
 */
void ec_destroy_ct(struct ec_ct *ct){
	int i = 0;
	int nprocs = -1;
	MPI_Comm_size(ct->comm, &nprocs);

	PP_CLOSE_FID(ct->ep);
	PP_CLOSE_FID(ct->rx.cq);
	PP_CLOSE_FID(ct->tx.cq);
	PP_CLOSE_FID(ct->av);
	PP_CLOSE_FID(ct->eq);
	PP_CLOSE_FID(ct->domain);
	PP_CLOSE_FID(ct->fabric);

	if(ct->comm){
		MPI_Comm_free(&ct->comm);
	}

	if(ct->remote_fi_addr){
		free(ct->remote_fi_addr);
	}

	if(ct->rem_name.rem_name_ptr)
		free(ct->rem_name.rem_name_ptr);
	if(ct->rem_name.rem_names)
		free(ct->rem_name.rem_names);
	if(ct->local_name)
		free(ct->local_name);
	ct->local_name = NULL;

	if (ct->buf) {
		free(ct->buf);
		ct->buf = ct->rx.buf = ct->tx.buf = NULL;
		ct->buf_size = ct->rx.size = ct->tx.size = 0;
	}
	if (ct->fi) {
		fi_freeinfo(ct->fi);
		ct->fi = NULL;
	}
	for(i = 0; i<nprocs; i++){
		if(ct->hints[i]) {
			fi_freeinfo(ct->hints[i]);
			ct->hints[i] = NULL;
		}
	}
	if(ct->hints)
		free(ct->hints);
}

/**
 * @brief Initialization of the context.
 * 
 * This method will override the data of the given context and set its values.
 * It will also alocate memory to the relevant attributes, based on the given communicator.
 * 
 * This operation is synchronizing and blocking operation because it exchanges names/addresses
 * between every process in the given MPI communicator.
 * 
 * This method must not be called on an already initalized context without using ec_destroy on it first. 
 * This would cause memory leaks.
 * 
 * @param[in] comm MPI Communicator defining the environment in which the context ct is relevant.
 * @param[out] ct ec_ct pointer, the context instance to initialize
 * @return int 0 if everything went well, for the possible errors please see the C errors code, MPI's and the libfabric ones.
 */
int ec_create_ct(MPI_Comm comm, struct ec_ct *ct){
	memset(ct, 0, sizeof(*ct));
	int ret = 0;
	int myrank = -1;
	int i = 0;
	int nprocs = -1;
	ret = MPI_Comm_rank(comm, &myrank);
	if(ret) return ret;
	ret = MPI_Comm_size(comm, &nprocs);
	if(ret) return ret;
	ret = MPI_Comm_dup(comm, &ct->comm);
	if(ret){
		MPI_Comm_free(&ct->comm);
	}

	ct->tries=5;
	ct->timeout_usec = 10000;

	ct->eq_attr.wait_obj = FI_WAIT_UNSPEC;
	ct->hints = (struct fi_info**) calloc(nprocs, sizeof(struct fi_info*));
	if(!ct->hints){
		ret = ENOMEM;
		goto freeinfo;
	}
	ct->rem_name.rem_name_ptr = (char**) calloc(nprocs, sizeof(char *));
	if(!ct->rem_name.rem_name_ptr){
		ret = ENOMEM;
		goto freeinfo;
	}
	for(i = 0; i<nprocs; i++){
		ct->hints[i] = fi_allocinfo();

		if (!ct->hints[i]){
			ret = EXIT_FAILURE;
			goto freeinfo;
		}
		ct->hints[i]->ep_attr->type = FI_EP_DGRAM;
		ct->hints[i]->mode = FI_CONTEXT | FI_MSG_PREFIX;
	#if defined FI_CONTEXT2
		ct->hints[i]->mode = ct->hints[i]->mode | FI_CONTEXT2;
	#endif

		ct->hints[i]->fabric_attr->prov_name = strdup("udp");
	}
	
    ret = pp_getinfo(ct, ct->hints[myrank], &(ct->fi));
	if (ret)
		goto freeinfo;

	ret = pp_open_fabric_res(ct);
	if (ret)
		goto freeinfo;

	ret = pp_alloc_active_res(ct, ct->fi, comm);
	if (ret)
		goto freeinfo;

	ret = pp_init_ep(ct);
	if (ret)
		goto freeinfo;

	if (ret){
		goto freeinfo;
	}
	ct->addrlen = 0;
	PP_DEBUG("Fetching local address\n");

	ct->local_name = NULL;

	ret = fi_getname(&ct->ep->fid, ct->local_name, &ct->addrlen);
	if ((ret != -FI_ETOOSMALL) || (ct->addrlen <= 0)) {
		PP_ERR("fi_getname didn't return length\n");
		ret = EMSGSIZE;
		goto freeinfo;
	}

	ct->rem_name.rem_names = (char*) calloc(nprocs, ct->addrlen);
	if(!ct->rem_name.rem_names){
		ret = ENOMEM;
		goto freeinfo;
	}
	for(i = 0; i<nprocs; i++){
		ct->rem_name.rem_name_ptr[i] = ct->rem_name.rem_names+(i*(ct->addrlen));
	}

	for(i = 0; i<nprocs; i++){
		ret = ec_send_recv_name(ct, &ct->ep->fid, (myrank-i)+((myrank-i)<0?nprocs:0), (myrank+i)%nprocs, comm);
		
		if (ret != 0)
			goto freeinfo;
	}
	ct->remote_fi_addr = (fi_addr_t*) calloc(nprocs, sizeof(fi_addr_t));

	ret = pp_av_insert(ct->av, (ct->rem_name.rem_names), nprocs, ct->remote_fi_addr, 0,
				NULL);
	if (ret)
		goto freeinfo;
		
	ret = (int) fi_recv(ct->ep, ct->rx.buf, MAX(ct->rx.size , (size_t) PP_MAX_CTRL_MSG) + ct->rx.prefix_size, NULL, 0, ct->rx.ctx_ptr);

	return 0;

freeinfo:
	ec_destroy_ct(ct);
	return ret;


}

/**
 * @brief Simple receive function
 * 
 * This is the most basic receive function, using ec_rx. It receives a single packet.
 * To access the packet's data one must read the ct.rx.buf buffer.
 * This function can be used as an optimized version of ec_receive when knowing that only
 * packet is going to be delivered.
 * 
 * After receiving this funciton sends in return the acknowledgment using the ec_tx method.
 * 
 * This function clears the rx.buf of the given context before operating.
 * 
 * @param[in] ct The context relevant to this receiving operation
 * @param[in] transfer_size The maximum transfer size (of a single message, set by default to ct.fi->tx_attr->inject_size)
 * @param[in] src_rank The rank supposed to send data toward this process.
 * @return int 0 if everything went well, !=0 if the receiving operation or the inject operation went wrong
 */
int ec_get(struct ec_ct *ct, size_t transfer_size){
	int ret;
	size_t timeout_usec = ct->timeout_usec;
	clear_bufx(ct->rx);
	ret = ec_rx(ct);
	struct ec_packet recv_packet;
	memcpy(&recv_packet, ct->rx.buf, sizeof(struct ec_packet));
	clear_bufx(ct->tx);
	ret = ec_tx(ct, ct->ep, transfer_size, recv_packet.src_rank);
	if(ret){
		ct->timeout_usec = timeout_usec;
		return ret;	
	}
	ct->timeout_usec = timeout_usec;
	return 0;
}

/**
 * @brief Simple send function (for a single packet)
 * 
 * This method is a basic send function, using the ec_tx function. 
 * It just sends a packet of a size equal to the minimum between buf_size 
 * and ec_packet_size(ct) with ct the given context.
 * This is an optmized version of ec_send when a single packet is to be sent.
 * 
 * It then waits for an aknoledgment using the ec_rx function to receive.
 * 
 * @param[in] ct The context relevant to this inject operation.
 * @param[in] transfer_size The maximum transfer size (of a single message, set by default to ct.fi->tx_attr->inject_size - sizeof(struct ec_packet)).
 * @param[in] buf The buffer containing the data to send.
 * @param[in] buf_size The size of the given buffer (size of the relevant part of this buffer).
 * @param[in] dest_rank The destination process' rank.
 * @return int 0 if everything went well, !=0 if not.
 */
int ec_inject(struct ec_ct *ct, size_t transfer_size, void *buf, size_t buf_size, int dest_rank, size_t packet_pos){
	int ret = 0;
	size_t tries = 0;
	int timeout_usec = ct->timeout_usec;
	struct ec_packet packet = {
		.pos = packet_pos,
		.dest_rank = dest_rank,
		.buf_size = buf_size
	};
	MPI_Comm_rank(ct->comm, &packet.src_rank);
	memset(ct->tx.buf, 0, transfer_size);
	memcpy(ct->tx.buf, &packet, sizeof(struct ec_packet));
	memcpy(ct->tx.buf+sizeof(struct ec_packet), buf, buf_size);
	ct->tx.size = buf_size;
	for(tries = 0;tries<ct->tries; tries++){
		ct->timeout_usec *= 2;
		ret = ec_tx(ct, ct->ep, MIN(transfer_size, sizeof(struct ec_packet)+buf_size), dest_rank);
		if(ret & (FI_ETIMEDOUT | ETIMEDOUT | FI_ENODATA | ENODATA)){
			continue;
		}
		else if(ret){
			tries--;
			if(!(ret & (FI_EAGAIN | EAGAIN))) abort();
		}

		ret = ec_rx(ct);
		if(ret & (FI_ETIMEDOUT | ETIMEDOUT | FI_ENODATA | ENODATA)){
			continue;
		}
		else if(ret & (FI_EAGAIN | EAGAIN)){
			tries--;
			continue;
		}
		else if(!ret && !(((int*)ct->rx.buf)[0])) break;
	}
	ct->timeout_usec = timeout_usec;
	return ret;
}

/**
 * @brief The receive operation
 * 
 * This function receives a message from a certain process, IDed by its rank.
 * It starts by receiving the size of the buffer and the size of a single packet
 * sent by the other process.
 * It then checks that the recv_buf_size is greater than the size of the
 * message, if not it returns EMSGSIZE.
 * Finally it loops until receiving every packet and will copy the data to the recvbuf.
 * 
 * @param[in] ct The relevant context for this receive operation
 * @param[in] transfer_size The maximum transfer size (of a single message, set by default to ct.fi->tx_attr->inject_size). 
 * @param[out] recvbuf The buffer (or pointer if size is null or negative) that should contain the received data after the operation
 * @param[in] recv_buf_size The size of the recvbuf (if negative or null, the buffer will be dynamically allocated)
 * @param[in] src_rank The rank of the process source of this message
 * @return int 0 if everything went well, EMSGSIZE if the recv_buf_size if smaller than the size of the message awaited,
 * for other values refer to the possible return values of ec_inject, ec_rx and ec_tx.
 */
int ec_recv(struct ec_ct *ct, size_t transfer_size, void *recvbuf, size_t recv_buf_size){
	int ret = ec_rx(ct);
	if(ret) return ret;
	size_t recv_size = ((size_t*)ct->rx.buf)[0]; 
	size_t packet_size = ((size_t*)ct->rx.buf)[1];
	if(recv_size>recv_buf_size){
		memset(ct->tx.buf, EMSGSIZE, sizeof(int));
		ec_tx(ct, ct->ep, sizeof(int), ((size_t*)ct->rx.buf)[2]);
		return EMSGSIZE;
	}
	else{
		memset(ct->tx.buf, 0, sizeof(int));
		ec_tx(ct, ct->ep, sizeof(int), ((size_t*)ct->rx.buf)[2]);
	}
	size_t number_of_packets = (recv_size + packet_size - 1)/packet_size;
	int i = 0;
	struct ec_packet recv_packet = {};
	for(i = 0; (size_t) i<number_of_packets; i++){
		ret = ec_get(ct, transfer_size);
		if(ret != 0){
			//TODO
			return ret;
		}
		else{
			memcpy(&recv_packet, ct->rx.buf, sizeof(struct ec_packet));
			memcpy(recvbuf+(recv_packet.pos*packet_size), ct->rx.buf+sizeof(struct ec_packet), recv_packet.buf_size);
		}
	}
	return 0;
}

/**
 * @brief The send operation
 * 
 * This function sends a message to a certain process, IDed by its rank.
 * It starts by sending the size of the message and the size of a packet (using ec_packet_size).
 * Then receives confirmation that these values are in accordance with what the process is waiting for.
 * Then if the message is sendable in a single operation it calls the ec_inject function.
 * If not, it loops to send every packet needed to transmit the full data using ec_tx.
 * 
 * @param[in] ct The relevant context for this send operation.
 * @param[in] transfer_size The maximum transfer size (of a single message, set by default to ct.fi->tx_attr->inject_size). 
 * @param[in] buf The buffer containing the data to be transmitted.
 * @param[in] buf_size The size of the buffer (of the relevant part of the buffer).
 * @param[in] dest_rank The rank of the destination process.
 * @param[in] comm The relevant MPI Communicator.
 * @return int 0 if everything went well, EMSGSIZE if the recv_buf_size if smaller than the size of the message awaited,
 * for other values refer to the possible return values of ec_inject, ec_rx and ec_tx..
 */
int ec_send(struct ec_ct *ct, size_t transfer_size, void *buf, size_t buf_size, int dest_rank){
	int ret = 0;
	size_t i = 0;
	size_t size = 0;
	size_t number_of_packets = 1;
	size_t packet_size = ec_packet_size(ct);
	int myrank = -1;
	MPI_Comm_rank(ct->comm, &myrank);
	memcpy(ct->tx.buf, &buf_size, sizeof(buf_size));
	memcpy(ct->tx.buf+sizeof(buf_size), &packet_size, sizeof(packet_size));
	memcpy(ct->tx.buf+sizeof(buf_size)+sizeof(packet_size), &myrank, sizeof((size_t) myrank));
	ret = ec_tx(ct, ct->ep, sizeof(buf_size)+sizeof(packet_size), dest_rank);
	if(ret) return ret;
	ret = ec_rx(ct);
	if(ret) return ret;
	if(((int *) ct->rx.buf)[0] == EMSGSIZE){return EMSGSIZE;}
	if(buf_size<packet_size){
		ret = ec_inject(ct, transfer_size, buf, buf_size, dest_rank, 0);
		if(ret) return ret;
	}
	else{
		number_of_packets = buf_size/packet_size + (buf_size%packet_size==0?0:1);
		for(i = 0; i<number_of_packets; i++){
			size = ((i+1)>=number_of_packets && (buf_size%packet_size!=0))?(buf_size%packet_size):packet_size;
			ret = ec_inject(ct, transfer_size, (buf+i*packet_size), size, dest_rank, i);
			if (ret)
				return ret;
		}
		return 0;
	}
	return -1;
}

int ec_handle_recv(struct ec_ct *ct, double **ack, char **buffers, size_t *buf_sizes, size_t *count, size_t packet_size, uint64_t *time_ack, uint64_t *time_msg, char recv_marker[][SEND_WINDOW_SIZE]){
	int myrank = -1;
	struct ec_packet pkt;
	int ret = 0;
	struct fi_cq_err_entry comp;
	int nprocs = -1;
	int ack_msg = -1;
	struct timespec recv_time;
	MPI_Comm_rank(ct->comm, &myrank);
	MPI_Comm_size(ct->comm, &nprocs);
	if(fi_cq_read(ct->rx.cq, &comp, 1)>0){
		memcpy(&pkt, ct->rx.buf, sizeof(struct ec_packet));
		if(pkt.src_rank >= nprocs) goto rank_error;
		if(pkt.dest_rank >= nprocs) goto rank_error;
		if(pkt.src_rank == myrank){
			if(pkt.pos<SEND_WINDOW_SIZE){
				ack_msg = ((int*) (ct->rx.buf+sizeof(struct ec_packet)))[0];
				if(!ack_msg && !ack[pkt.dest_rank][pkt.pos]){
					clock_gettime(CLOCK_MONOTONIC, &recv_time);
					ack[pkt.dest_rank][pkt.pos] = (recv_time.tv_sec - pkt.time_sent.tv_sec) + 1e-9*(recv_time.tv_nsec - pkt.time_sent.tv_nsec);
					ct->cnt_ack_msg++;
					*(time_ack) = pp_gettime_us();
				}
			}
		}
		else if(pkt.dest_rank == myrank){
			if(pkt.buf_size>(buf_sizes[pkt.src_rank]-pkt.pos*packet_size)){
				size_t remaining_buf_space = buf_sizes[pkt.src_rank]-pkt.pos*packet_size;
				clear_bufx(ct->tx);
				memcpy(ct->tx.buf, &pkt, sizeof(struct ec_packet));
				memset(ct->tx.buf+sizeof(struct ec_packet), FI_EMSGSIZE, sizeof(FI_EMSGSIZE));
				memcpy(ct->tx.buf+sizeof(struct ec_packet)+sizeof(FI_EMSGSIZE), (void*) &remaining_buf_space, sizeof(size_t));
				fi_inject(ct->ep, ct->tx.buf, sizeof(size_t) + sizeof(struct ec_packet), ct->remote_fi_addr[pkt.src_rank]);
				// ret = -FI_EMSGSIZE;
				fprintf(stderr, "Receiving buffer size insufficient, receiving rank : %d, sending rank : %d.\n", pkt.src_rank, pkt.dest_rank);
				fprintf(stderr, "Position of packet sent : %lu, size of packet : %lu, size remaining in buffer : %lu\n", pkt.pos, pkt.buf_size, remaining_buf_space);
				abort();
			} else{
				if(!recv_marker[pkt.src_rank][pkt.pos]){
					recv_marker[pkt.src_rank][pkt.pos] = 1;
					(*count)++;
				}
				memcpy(buffers[pkt.src_rank]+pkt.pos*packet_size, ct->rx.buf+sizeof(struct ec_packet), pkt.buf_size);
				clear_bufx(ct->tx);
				memcpy(ct->tx.buf, &pkt, sizeof(struct ec_packet));
				
				ret = fi_inject(ct->ep, ct->tx.buf, sizeof(size_t) + sizeof(struct ec_packet), ct->remote_fi_addr[pkt.src_rank]);
				if(ret){
						fi_recv(ct->ep, ct->rx.buf, MAX(ct->rx.size , (size_t) PP_MAX_CTRL_MSG) + ct->rx.prefix_size, NULL, 0, ct->rx.ctx_ptr);
						return ret;
				}
				(*time_msg) = pp_gettime_us();
			}
		}
		else{
			clear_bufx(ct->tx);
			memcpy(ct->tx.buf, &pkt, sizeof(struct ec_packet));
			memset(ct->tx.buf + sizeof(struct ec_packet), FI_EINVAL, sizeof(FI_EINVAL));
			fi_inject(ct->ep, ct->tx.buf, sizeof(size_t) + sizeof(struct ec_packet), ct->remote_fi_addr[pkt.src_rank]);
			// ret = -FI_EINVAL;
			fprintf(stderr, "Receiving data corrupted (or wrond destination) process rank : %d, receiving rank : %d, sending rank : %d.\n", myrank, pkt.src_rank, pkt.dest_rank);
			abort();
		}
		ret = fi_recv(ct->ep, ct->rx.buf, MAX(ct->rx.size , (size_t) PP_MAX_CTRL_MSG) + ct->rx.prefix_size, NULL, 0, ct->rx.ctx_ptr);
	}

	return ret;

rank_error :
	fi_recv(ct->ep, ct->rx.buf, MAX(ct->rx.size , (size_t) PP_MAX_CTRL_MSG) + ct->rx.prefix_size, NULL, 0, ct->rx.ctx_ptr);
	return -1;
}

size_t find(double *tab, size_t size, double f){
	size_t i = 0;
	for(i = 0; i<size; i++)
		if(tab[i] == f)
			return i;
	return size+1;
}

/**
 * @brief Multi send-recv function, sends nb_send messages + nb_recv acknowledgments and receives nb_send acknowledgments + nb_recv messages
 * 
 * This function sends and receives multiple messages from multiple processes.
 * This can be used as a synchronization if every process end up sending and receiving from every other process.
 * 
 * This function uses fi_inject at every loop then checks for event in the recv completion queue
 * in the handle_recv function. So the main point of this function is to use at the same time the send
 * operation and the receive operation.
 * 
 * WARNING:The nb_recv is the number of packets to receive and not the number of messages, so an exchange of these information
 * should happend before using this method if the number of packets is not known.
 * 
 * @param[in] ct The relevant context instance
 * @param[in] ep The fabric identifier of an endpoint (usually ct.ep)
 * @param[in] nb_send The number of messages to be sent (the number of buffers in the parameter msg_to_send)
 * @param[in] nb_recv The number of packets to receive
 * @param[in] send_order The ordered list of ranks to which send the messages in msg_to_send
 * @param[in] msg_to_send The actual messages to send
 * @param[in] msg_sizes Each size_t in this array is the size of the corresponding array in msg_to_send
 * @param[out] buffers The receiving buffers corresponding to every rank 
 * (when received, a message is written in the buffer at the index equal to the rank of the source)
 * @param[in] buf_sizes Each size_t in this array must correspond to the size of the corresponding buffer in buffers
 * @return int 0 if everything went well, 
 *         for other values please see the return values of fi_inject and fi_recv
 *         If an error of programming happens (bad negociation of the packet sizes, bad buffer size, etc.) the program is aborted. 
 */
int ec_multi_inject_recv(struct ec_ct *ct, struct fid_ep *ep, size_t nb_send, size_t nb_recv, int *send_order, 
						 char **msg_to_send, size_t *msg_sizes, char **buffers, size_t *buf_sizes, double **packets_to_send){
	int  ret;
	size_t tries;
	int myrank = -1, nprocs = -1;
	MPI_Comm_rank(ct->comm, &myrank);
	MPI_Comm_size(ct->comm, &nprocs);
	size_t nb_packets, received = 0;
	size_t packet_size = ec_packet_size(ct);
	uint64_t t_ack, t_msg;
	void *msg;
	// char packets_to_send[SEND_WINDOW_SIZE];
	char packets_to_receive[nprocs][SEND_WINDOW_SIZE];
	size_t i = 0;
	for(i = 0; i<(size_t) nprocs; i++){
		memset(packets_to_receive[i], 0, SEND_WINDOW_SIZE);
	}
	int timeout_save = ct->timeout_usec;
	int dest_rank;
	for(i = 0; i<nb_send; i++){
		size_t j = 0, window_id = 0;
		ct->timeout_usec = timeout_save;
		tries = 0;
		msg = msg_to_send[i];
		dest_rank = send_order[i];

		assert(dest_rank < nprocs);
		assert(dest_rank >= 0);

		nb_packets = (msg_sizes[i] + packet_size - 1)/packet_size;
	
		struct timespec s_time;
		
		for(window_id = 0;window_id<=nb_packets/SEND_WINDOW_SIZE; window_id++){
			memset(packets_to_send[dest_rank], 0, sizeof(double)*SEND_WINDOW_SIZE);
			t_ack = pp_gettime_us();

			while((find(packets_to_send[dest_rank], SEND_WINDOW_SIZE, (double) 0) < SEND_WINDOW_SIZE) && (j<nb_packets) && (tries < ct->tries)){
				clock_gettime(CLOCK_MONOTONIC, &s_time);
				struct ec_packet packet = {
					.src_rank = myrank,
					.dest_rank = dest_rank,
					.buf_size = MIN(packet_size, msg_sizes[i]-j*packet_size),
					.pos = j+(SEND_WINDOW_SIZE*window_id),
					.time_sent = s_time
				};
				memcpy(ct->tx.buf, &packet, sizeof(packet));
				assert(((SEND_WINDOW_SIZE*window_id)+j)*packet_size<buf_sizes[i]);
				memcpy(ct->tx.buf+sizeof(packet), (msg + ((SEND_WINDOW_SIZE*window_id)+j)*packet_size), packet.buf_size);
				if(fi_inject(ep, ct->tx.buf, packet.buf_size+sizeof(packet), ct->remote_fi_addr[dest_rank])) j--;
				ret = ec_handle_recv(ct, packets_to_send, buffers, buf_sizes, &received, packet_size, &t_ack, &t_msg,  packets_to_receive);
				if(ret) goto end;
				if(j == nb_packets-1 || j == SEND_WINDOW_SIZE-1){
					while(pp_gettime_us()-t_ack <= ct->timeout_usec){
						ret = ec_handle_recv(ct, packets_to_send, buffers, buf_sizes, &received, packet_size, &t_ack, &t_msg,  packets_to_receive);
						if(ret) goto end;
					}
				}
				if((pp_gettime_us() - t_ack > ct->timeout_usec) && (find(packets_to_send[dest_rank], SEND_WINDOW_SIZE, (double) 0))){
					tries++;
					ct->timeout_usec*=2;
					j = find(packets_to_send[dest_rank], SEND_WINDOW_SIZE, (double) 0)-1;
					t_ack = pp_gettime_us();
				}
				j++;
			}
		}
	}
	t_msg = pp_gettime_us();
	while(received<nb_recv && pp_gettime_us()-t_msg<((int) pow(2, ct->tries))*ct->timeout_usec){
		ret = ec_handle_recv(ct, packets_to_send, buffers, buf_sizes, &received, packet_size, &t_ack, &t_msg,  packets_to_receive);
		if(ret) goto end;
	}
	
end:
	ct->timeout_usec = timeout_save;
	return ret;
}
