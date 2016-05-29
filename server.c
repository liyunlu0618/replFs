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
	uint32_t fd;
	uint32_t commit_no;
	uint32_t write_no;
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
	srand(time(NULL));
	server_id = rand();
	debug_printf("server_id: %d\n", server_id);
	struct stat s;
	int ret;

	pkt_drop = drop_ratio;
	server_mountpoint = strdup(mountpoint);

	int i = 0;
	for (i = 0; i < MAXWRITES; i++)
		write_log[i] = NULL;

	ret = stat(mountpoint, &s);
	if (ret == 0 || errno != ENOENT) {
		printf("machine already in use\n");
		return -1;
	}

	ret = mkdir(mountpoint, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

	if (ret != 0) return -1;

	return network_init(port, &server_addr, &server_sock);
}

static void
server_send_open_ack()
{
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
	server_send_open_ack();
	return 0;
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
		break;

		case PKT_CHECK:
		break;

		case PKT_COMMIT:
		break;

		case PKT_ABORT:
		break;

		default:
		break;
		}
	}

	return 0;
}
