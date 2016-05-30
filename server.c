#include "network.h"
#include "packet.h"

#define MAXWRITES	64
#define REPLFSPORT	44055
#define MAXPACKETSIZE	1024

static struct sockaddr server_addr;
static int server_sock;
static uint32_t server_id;
static char *server_mountpoint;
static int pkt_drop;
static uint32_t open_fd;
static char open_fname[MAXNAMELEN];

typedef struct write_request {
	uint32_t byte_offset;
	uint32_t blocksize;
	char buffer[MAXBLOCKLEN];
} write_request_t;

static write_request_t* write_log[MAXWRITES];

static bool
has_open_file()
{
	return open_fname[0] == '\0';
}

static bool
is_empty_log()
{
	int i = 0;

	for ( ; i < MAXWRITES; i++) {
		if (write_log[i] != NULL) return FALSE;
	}

	return TRUE;
}

static void
clear_log()
{
	int i = 0;

	for ( ; i < MAXWRITES; i++) {
		free(write_log[i]);
		write_log[i] = NULL;
	}
}

static int
server_init(unsigned short port, int drop_ratio, char *mountpoint)
{
	struct timeval time;
	gettimeofday(&time, NULL);

	srand((time.tv_sec * 1000) + (time.tv_usec / 1000));
	while ((server_id = rand()) == 0)
		server_id = rand();

	debug_printf("server_id: %d\n", server_id);
	struct stat s;
	int ret;

	pkt_drop = drop_ratio;
	server_mountpoint = strdup(mountpoint);

	ret = stat(mountpoint, &s);
	if (ret == 0 || errno != ENOENT) {
		printf("machine already in use\n");
		return -1;
	}

	ret = mkdir(mountpoint, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

	if (ret != 0) {
		printf("server %d cannot make directory\n", server_id);
		return -1;
	}

	return network_init(port, &server_addr, &server_sock);
}

static void
server_send_open_ack()
{
	debug_printf("server send open ack\n");
	pkt_openack_t out;

	out.type = htonl(PKT_OPENACK);
	out.server_id = htonl(server_id);
	out.fd = htonl(open_fd);

	sendto(server_sock, &out, sizeof (out), 0, &server_addr, sizeof (struct sockaddr));
}

static int
server_process_pkt_open(void *packet)
{
	pkt_open_t *p = (pkt_open_t *)packet;

	if (has_open_file()) {
		if (!is_empty_log()) return -1;

		open_fname[0] = '\0';
	}

	open_fd = ntohl(p->fd);
	strncpy(open_fname, p->filename, MAXNAMELEN);
	debug_printf("server recv'd open file request: %s, fd: %d\n", p->filename, open_fd);
	server_send_open_ack();
	return 0;
}

static void
server_process_pkt_write(void *packet)
{
	pkt_write_t *p = (pkt_write_t *) packet;
	p->fd = ntohl(p->fd);
	p->write_no = ntohl(p->write_no);
	p->byte_offset = ntohl(p->byte_offset);
	p->blocksize = ntohl(p->blocksize);

	if (p->fd != open_fd || write_log[p->write_no] != NULL) {
		debug_printf("wrong write request, ignored\n");
		return;
	}

	write_request_t *wr = calloc (sizeof (char), sizeof (write_request_t));
	if (wr == NULL) return;

	wr->byte_offset = p->byte_offset;
	wr->blocksize = p->blocksize;
	memcpy(wr->buffer, p->data, p->blocksize);
	write_log[p->write_no] = wr;
	debug_printf("server write no %d\n", p->write_no);
}

static void
server_process_pkt_check(void *packet)
{
	int i = 0;
	int missing[2];
	memset(missing, 0, sizeof (missing));
	pkt_checkres_t out;

	pkt_check_t *p = (pkt_check_t *)packet;
	p->fd = ntohl(p->fd);
	p->num_writes = ntohl(p->num_writes);

	for (i = 0; i < p->num_writes; i++) {
		if (write_log[i] == NULL) {
			if (i < 32)
				missing[0] |= (1 << i);
			else
				missing[1] |= (1 << (i - 32));
		}
	}

	out.type = htonl(PKT_CHECKRES);
	out.server_id = htonl(server_id);
	out.fd = htonl(open_fd);
	out.missing[0] = htonl(missing[0]);
	out.missing[1] = htonl(missing[1]);
	sendto(server_sock, &out, sizeof (out), 0, &server_addr, sizeof (struct sockaddr));
}

static void
server_do_write()
{
	char *fullpath;
	write_request_t *wr;
	int fd;
	int i = 0;

	int len = strlen(server_mountpoint) + strlen(open_fname) + 2;
	fullpath = calloc(sizeof (char), len);
	snprintf(fullpath, len, "%s/%s", server_mountpoint, open_fname);

	fd = open(fullpath, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR);

	for (i = 0; i < MAXWRITES; i++) {
		if ((wr = write_log[i]) == NULL) break;
		lseek(fd, wr->byte_offset, SEEK_SET);
		write(fd, wr->buffer, wr->blocksize);
	}

	close(fd);
}

static void
server_process_pkt_commit(void *packet)
{
	pkt_commit_t *p = (pkt_commit_t *)packet;
	pkt_commitabortack_t out;

	p->fd = ntohl(p->fd);

	if (p->fd != open_fd)
		return;

	server_do_write();
	clear_log();

	out.type = htonl(PKT_COMMITABORTACK);
	out.server_id = htonl(server_id);
	out.fd = htonl(open_fd);
	sendto(server_sock, &out, sizeof (out), 0, &server_addr, sizeof (struct sockaddr));
}

static void
server_process_pkt_abort(void *packet)
{
	pkt_abort_t *p = (pkt_abort_t *)packet;
	pkt_commitabortack_t out;

	p->fd = ntohl(p->fd);
	if (p->fd != open_fd) return;

	clear_log();
	out.type = htonl(PKT_COMMITABORTACK);
	out.server_id = htonl(server_id);
	out.fd = htonl(open_fd);

	sendto(server_sock, &out, sizeof (out), 0, &server_addr, sizeof (struct sockaddr));
}

int
main(int argc, char *argv[])
{
	unsigned short port;
	int drop;
	char packet[MAXPACKETSIZE];
	memset(packet, 0, sizeof (packet));
	
	if (argc != 7) {
		printf("Usage: replFsServer -port <portnum> -mount <mount_path> -drop <drop_ratio>\n");
		exit(-1);
	}

	port = atoi(argv[2]);
	drop = atoi(argv[6]);

	if (server_init(port, drop, argv[4]) < 0)
		exit (-1);

	while (1) {
		if (network_recvfrom(server_sock, packet, sizeof (packet),
			0, NULL, NULL, pkt_drop) <= 0)
			continue;

		pkt_header_t *ph = (pkt_header_t *)packet;

		switch (ntohl(ph->type)) {

		case PKT_OPEN:
			server_process_pkt_open(packet);
		break;

		case PKT_WRITE:
			server_process_pkt_write(packet);
		break;

		case PKT_CHECK:
			server_process_pkt_check(packet);
		break;

		case PKT_COMMIT:
			server_process_pkt_commit(packet);
		break;

		case PKT_ABORT:
			server_process_pkt_abort(packet);
		break;

		default:
		break;
		}
	}

	return 0;
}
