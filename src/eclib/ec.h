/*
 * 	Copyright (c) 2021 Huawei Technologies Co., Ltd.
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


/**********************************************************************************************************
 * This file is the contain the "first layer" operation.
 * The operation are :
 * @tableofcontents
 **********************************************************************************************************/

#ifndef EC_SRC_H
#define EC_SRC_H

#include <mpi.h>	
#include <rdma/fabric.h>

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
int ec_create_ct(MPI_Comm comm, struct ec_ct *ct);

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
void ec_destroy_ct(struct ec_ct *ct);

/**
 * @brief Simple receive function (for a single packet)
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
int ec_get(struct ec_ct *ct, size_t transfer_size, int src_rank);

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
int ec_inject(struct ec_ct *ct, size_t transfer_size, void *buf, size_t buf_size, int dest_rank, size_t packet_pos);

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
int ec_recv(struct ec_ct *ct, size_t transfer_size, void *recvbuf, size_t recv_buf_size);

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
int ec_send(struct ec_ct *ct, size_t transfer_size, void *buf, size_t buf_size, int dest_rank);

/**
 * @brief Clear the buffer of the given ec_c_info structure
 * 
 * @param[out] X The ec_x_info instance needing its buffer cleared 
 */
void clear_bufx(struct ec_x_info X);

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
int ec_multi_inject_recv(struct ec_ct *ct, struct fid_ep *ep, size_t nb_send, size_t nb_recv, int *send_order, char **msg_to_send, size_t *msg_sizes, char **buffers, size_t *buf_sizes, double **packets_to_send);

#endif