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

typedef struct write_request {
	uint32_t fd;
	uint32_t commit_no;
	uint32_t write_no;
	uint32_t byte_offset;
	uint32_t blocksize;
	char buffer[MAXBLOCKLEN];
} write_request_t;

static write_request_t* write_log[MAXWRITES];

static int
server_init(unsigned short port, int drop_ratio, char *mountpoint)
{
	srand(time(NULL));
	server_id = rand();
	debug_printf("server_id: %d\n", server_id);

	pkt_drop = drop_ratio;
	server_mountpoint = strdup(mountpoint);

	int i = 0;
	for (i = 0; i < MAXWRITES; i++)
		write_log[i] = NULL;

	return network_init(port, &server_addr, &server_sock);
}

static void
server_process_pkt_init()
{
	debug_printf("server process pkt init\n");
	pkt_initack_t out;

	out.type = htonl(PKT_INITACK);
	out.server_id = htonl(server_id);
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

		pkt_init_t *pi = (pkt_init_t *)packet;

		switch (ntohl(pi->type)) {

		case PKT_INIT:
			server_process_pkt_init();
		break;

		case PKT_OPEN:
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
