#ifndef SRC_PP_H
#define SRC_PP_H

#include <stdint.h>
#include <sys/socket.h>

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
#include <mpi.h>

#define PP_STR_LEN 32
#define PP_MAX_CTRL_MSG 64
#define PP_CTRL_BUF_LEN 64
#define PP_MR_KEY 0xC0DE
#define PP_MAX_ADDRLEN 1024

#define INTEG_SEED 7
#define PP_ENABLE_ALL (~0)
#define PP_DEFAULT_SIZE (1 << 0)

#define PP_MSG_CHECK_PORT_OK "port ok"
#define PP_MSG_LEN_PORT 5
#define PP_MSG_CHECK_CNT_OK "cnt ok"
#define PP_MSG_LEN_CNT 10
#define PP_MSG_SYNC_Q "q"
#define PP_MSG_SYNC_A "a"

#ifndef OFI_MR_BASIC_MAP
#define OFI_MR_BASIC_MAP (FI_MR_ALLOCATED | FI_MR_PROV_KEY | FI_MR_VIRT_ADDR)
#endif

#ifndef SEND_WINDOW_SIZE
#define SEND_WINDOW_SIZE 1024
#endif

#ifndef SOCKET
#define SOCKET int
#endif

#ifndef OFI_MR_BASIC_MAP
#define OFI_MR_BASIC_MAP (FI_MR_ALLOCATED | FI_MR_PROV_KEY | FI_MR_VIRT_ADDR)
#endif

#define PP_SIZE_MAX_POWER_TWO 22
#define PP_MAX_DATA_MSG                                                        \
	((1 << PP_SIZE_MAX_POWER_TWO) + (1 << (PP_SIZE_MAX_POWER_TWO - 1)))

#define PP_PRINTERR(call, retv)                                                \
	fprintf(stderr, "%s(): %s:%-4d, ret=%d (%s)\n", call, __FILE__,        \
		__LINE__, (int)retv, fi_strerror((int) -retv))

#define PP_ERR(fmt, ...)                                                       \
	fprintf(stderr, "[%s] %s:%-4d: " fmt "\n", "error", __FILE__,          \
		__LINE__, ##__VA_ARGS__)

extern int pp_debug;

#define PP_DEBUG(fmt, ...)                                                     \
	do {                                                                   \
		if (pp_debug) {                                                \
			fprintf(stderr, "[%s] %s:%-4d: " fmt, "debug",         \
				__FILE__, __LINE__, ##__VA_ARGS__);            \
		}                                                              \
	} while (0)

#define PP_CLOSE_FID(fd)                                                       \
	do {                                                                   \
		int ret;                                                       \
		if ((fd)) {                                                    \
			ret = fi_close(&(fd)->fid);                            \
			if (ret)                                               \
				PP_ERR("fi_close (%d) fid %d", ret,            \
				       (int)(fd)->fid.fclass);                 \
			fd = NULL;                                             \
		}                                                              \
	} while (0)

#ifndef MAX
#define MAX(a, b)                                                              \
	({                                                                     \
		typeof(a) _a = (a);                                            \
		typeof(b) _b = (b);                                            \
		_a > _b ? _a : _b;                                             \
	})
#endif

#ifndef MIN
#define MIN(a, b)                                                              \
	({                                                                     \
		typeof(a) _a = (a);                                            \
		typeof(b) _b = (b);                                            \
		_a < _b ? _a : _b;                                             \
	})
#endif

enum precision {
	NANO = 1,
	MICRO = 1000,
	MILLI = 1000000,
};

enum {
	PP_OPT_ACTIVE = 1 << 0,
	PP_OPT_ITER = 1 << 1,
	PP_OPT_SIZE = 1 << 2,
	PP_OPT_VERIFY_DATA = 1 << 3,
};	

struct ec_x_info{
	struct fid_cq *cq;
	uint64_t seq, cq_cntr;
	struct fi_context ctx[2];
	size_t size;
	size_t prefix_size;
	void *buf;
	void *ctx_ptr;
};

struct ec_packet{
	int src_rank, dest_rank;
	size_t pos;
	size_t buf_size;
	struct timespec time_sent;
};

struct contiguous_rem_names{
	char **rem_name_ptr;
	char *rem_names;
};

struct ec_ct {
	MPI_Comm comm;

	struct fi_info *fi, **hints;
	struct fid_fabric *fabric;
	struct fid_domain *domain;
	struct fid_ep *ep;
	struct fid_av *av;
	struct fid_eq *eq;

	uint64_t remote_cq_data;

	fi_addr_t local_fi_addr, *remote_fi_addr;
	void *buf;
	size_t buf_size;

	uint64_t timeout_usec;
	size_t tries;
	uint64_t start, end;

	struct fi_av_attr av_attr;
	struct fi_eq_attr eq_attr;
	struct fi_cq_attr cq_attr;

	long cnt_ack_msg;

	char ctrl_buf[PP_CTRL_BUF_LEN + 1];

	char *local_name;
	struct contiguous_rem_names rem_name;
	size_t addrlen;

	unsigned char success_op;

	struct ec_x_info tx, rx;
};

void pp_free_res(struct ec_ct *ct, MPI_Comm comm);
uint64_t pp_gettime_us(void);
ssize_t ec_rx(struct ec_ct *ct);
ssize_t ec_tx(struct ec_ct *ct, struct fid_ep *ep, size_t size, int dest_rank);
int ec_init_fabric(struct ec_ct *ct, MPI_Comm comm);
int pp_finalize(struct ec_ct *ct, MPI_Comm comm);
int ec_send_recv_name(struct ec_ct *ct,struct fid *endpoint, int src_rank, int dest_rank, MPI_Comm comm);
int pp_av_insert(struct fid_av *av, void *addr, size_t count, fi_addr_t *fi_addr, uint64_t flags, void *context);
int pp_getinfo(struct ec_ct *ct, struct fi_info *hints, struct fi_info **info);
int pp_open_fabric_res(struct ec_ct *ct);
int pp_alloc_active_res(struct ec_ct *ct, struct fi_info *fi, MPI_Comm comm);
int pp_init_ep(struct ec_ct *ct);
int pp_cq_readerr(struct fid_cq *cq);
void pp_start(struct ec_ct *ct);
void pp_stop(struct ec_ct *ct);
size_t ec_packet_size(struct ec_ct *ct);

#endif