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

#include <mpi.h>
#include "pingpong.h"
#include "perfs.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <rdma/fabric.h>
#include "ec.h"
#include <math.h>

char error_happend = 0;

int point_to_point_get_inject_test(struct ec_ct *ct, size_t iterations, FILE *fptr, FILE *efptr){
	int myrank = -1, src_rank, dest_rank;
	size_t i = -1;
	int ret = MPI_Comm_rank(ct->comm, &myrank);
	if(ret) return ret;
	int nprocs = -1;
	ret = MPI_Comm_size(ct->comm, &nprocs);
	if(ret) return ret;
	size_t transfer_size = ct->fi->tx_attr->inject_size-sizeof(struct ec_packet);
	size_t buf_size = (transfer_size+sizeof(int)-1);
	int buf[buf_size/sizeof(int)];
	for(i = 0; i<buf_size/sizeof(int); i++){
		buf[i] = rand();
	}

	clear_bufx(ct->tx);
	clear_bufx(ct->rx);

	for(src_rank = 0; src_rank<nprocs; src_rank++){
		for(dest_rank = 0; dest_rank<nprocs; dest_rank++){
			if(myrank == dest_rank || myrank == src_rank){
				pp_start(ct);
				if(myrank == dest_rank){
					/*"SERVER"*/
					for(i = 0; i<iterations;i++){
						ret = ec_get(ct, transfer_size);
						if(ret) return ret;
					}
					sleep(0);
				}
				else if (myrank == src_rank){
					/*"CLIENT"*/
					for(i = 0; i<iterations; i++){
						ret = ec_inject(ct, transfer_size, buf, buf_size, dest_rank, 0);
						if(ret) return ret;
					}
				}

				pp_stop(ct);
				if(myrank == src_rank || myrank == dest_rank){
					fprintf(fptr, "\n\n%d->%d, ec_inject, %lu*char :\n", src_rank, dest_rank, buf_size/sizeof(char));
					show_perf(NULL, buf_size, iterations, -ct->cnt_ack_msg, ct->start, ct->end, 2, fptr);
				}
				if(myrank == dest_rank){
					for(i = 0; i<buf_size/sizeof(int); i++){
						if(((int*) (ct->rx.buf + sizeof(struct ec_packet)))[i]!=buf[i]){
							fprintf(efptr, "Problem test point to point ec_inject/ce_get, %d->%d: %d instead of %d\n",src_rank, dest_rank, ((int*) (ct->rx.buf + sizeof(struct ec_packet)))[i], buf[i]);
							error_happend = 1;
						}
					}
				}
			}
			MPI_Barrier(ct->comm);
		}
	}
	return 0;
}

int point_to_point_test(struct ec_ct *ct, size_t iterations, FILE *fptr, size_t buf_size, FILE *efptr){  
	int myrank = -1, src_rank, dest_rank;
	size_t i = -1;
	int ret = MPI_Comm_rank(ct->comm, &myrank);
	if(ret) return ret;
	int nprocs = -1;
	ret = MPI_Comm_size(ct->comm, &nprocs);
	if(ret) return ret;
	size_t transfer_size = ct->fi->tx_attr->inject_size-sizeof(struct ec_packet);
	int recvbuf[(buf_size + sizeof(int) - 1)/sizeof(int)];
	int buf[(buf_size + sizeof(int) - 1)/sizeof(int)];
	for(i = 0; i<(buf_size + sizeof(int) - 1)/sizeof(int); i++){
		buf[i] = rand();
	}

	clear_bufx(ct->tx);
	clear_bufx(ct->rx);

	for(src_rank = 0; src_rank<nprocs; src_rank++){
		for(dest_rank = 0; dest_rank<nprocs; dest_rank++){
			if(myrank == dest_rank || myrank == src_rank){
				pp_start(ct);

				if(myrank == dest_rank){
					/*"SERVER"*/
					for(i = 0; i<iterations;i++){
						ret = ec_recv(ct, transfer_size, &recvbuf, buf_size);
						if(ret) return ret;
					}
					sleep(0);
				}
				else if(myrank == src_rank){
					/*"CLIENT"*/
					for(i = 0; i<iterations; i++){
						ret = ec_send(ct, transfer_size, 
						buf, buf_size, dest_rank);
						if(ret) return ret;
					}
				}
				pp_stop(ct);
				if(myrank == src_rank || myrank == dest_rank){
					fprintf(fptr, "\n\n%d->%d, ec_send, %lu*char :\n", src_rank, dest_rank, buf_size/sizeof(int));
					show_perf(NULL, buf_size, iterations, -ct->cnt_ack_msg, ct->start, ct->end, 2, fptr);
				}
				if(myrank == dest_rank){
					for(i = 0; i<(buf_size + sizeof(int) - 1)/sizeof(int); i++){
						if(recvbuf[i]!=buf[i]){
							fprintf(efptr, "Problem test point to point ec_send/ec_recv, %d->%d: %d instead of %d\n", src_rank, dest_rank, recvbuf[i], buf[i]);
							error_happend = 1;
						}
					}
				}
			}
			MPI_Barrier(ct->comm);
		}
	}
	ct->cnt_ack_msg = 0;
	return 0;
}

int a2a_latin_square_test(struct ec_ct *ct, size_t iterate, FILE *fptr, FILE *efptr){
	int i = 0, j = 0;
	int nprocs = -1;
	int myrank = -1;
	int ret = MPI_Comm_size(ct->comm, &nprocs);
	if(ret) return ret;
	ret = MPI_Comm_rank(ct->comm, &myrank);
	if(ret) return ret;
	size_t iter = 0;
	char error_happend_now = 0;

	int msg1[1500];
	int send_order[nprocs];
	int *msg_to_send[nprocs];
	size_t msg_sizes[nprocs];
	int *buffers[nprocs];

	for(i = 0; i<1500; i++){
		msg1[i] = i;
	}
	for(i = 1; i<nprocs; i++){
		send_order[i-1] = (myrank+i)%nprocs;
		msg_to_send[i-1] = msg1;
	}
	for(i = 0; i<nprocs; i++){
		msg_sizes[i] = 1500*sizeof(int);
		buffers[i] = (int*) calloc(1500, sizeof(int));
	}

	double **packets_time = (double **) calloc(nprocs, sizeof(double*));
	for(i = 0; i<nprocs; i++){
		packets_time[i] = (double*) calloc(SEND_WINDOW_SIZE, sizeof(double));
	}
	for(iter = 0; iter<iterate; iter++){
		pp_start(ct);
			ec_multi_inject_recv(ct, ct->ep, (size_t) nprocs-1, (size_t) nprocs-1, send_order, (char**) msg_to_send, msg_sizes, (char**) buffers, msg_sizes, packets_time);
		pp_stop(ct);
	
		for(j = 0; j<nprocs; j++){
			for(i = 0; i<SEND_WINDOW_SIZE; i++){
				if(packets_time[j][i]){
					fprintf(fptr, "%2.15lf,", packets_time[j][i]);
				}
			}
			fprintf(fptr, "\n");
		}

		for(i = 0; i<nprocs; i++){
			if(i==myrank) continue;
			for(j = 0; j<1500; j++){
				if(buffers[i][j]!=msg1[j]) {
					fprintf(efptr, "rank = %d, error in buffer[%d]? %d instead of %d\n", myrank, i, buffers[i][j], msg1[j]);
					error_happend = 1;
					error_happend_now = 1;
				}
			}
		}

		show_perf(NULL, 1500*sizeof(int), nprocs-1, ct->cnt_ack_msg, ct->start, ct->end, 2*nprocs-2, fptr);
		if(error_happend_now) fprintf(stderr, "error occured on try %lu", iter);

	}

	for(i = 0; i<nprocs; i++){
		free(buffers[i]);
		free(packets_time[i]);
	
	}
	free(packets_time);

	return 0;
}

int main(int argc, char** argv){
	
	MPI_Comm comm = MPI_COMM_WORLD;
    int myrank = -1;
    int ret = MPI_Init(&argc, &argv);
	if(ret) return ret;
	ret = MPI_Comm_rank(comm, &myrank);
	if(ret) return ret;
	int nprocs = -1;
	ret = MPI_Comm_size(comm, &nprocs);
	if(ret) return ret;

	char filename[20] = "";
	sprintf(filename, "perfs%d.txt", myrank);
	char error_filename[20] = "";
	sprintf(error_filename, "error%d.txt", myrank);

	FILE *fptr = fopen(filename, "w");
	FILE *efptr = fopen(error_filename, "w");

	struct ec_ct ct;

	ret = ec_create_ct(comm, &ct);

	if(ret){
		return ret;
	}

	size_t iterations = 1;
	a2a_latin_square_test(&ct, iterations, fptr, efptr);

	ret = pp_finalize(&ct, comm);
	if(ret) fprintf(stderr, "problem finalizing, ret = %d.\n", ret);
	ec_destroy_ct(&ct);
	fflush(fptr);
	fflush(efptr);
	ret = fclose(fptr);
	if(ret) fprintf(stderr, "could not close file %s.\n", filename);
	ret = fclose(efptr);
	if(ret) fprintf(stderr, "could not close file %s.\n", error_filename);
	if(!error_happend){remove(error_filename);}
	MPI_Barrier(comm);
	MPI_Finalize();
	return -ret;
}