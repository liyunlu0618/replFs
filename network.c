#include "network.h"

int
network_init(unsigned short port, struct sockaddr *addr, int *sock)
{
	struct sockaddr_in nulladdr;
	int nullsock;
	int reuse = 1;
	u_char ttl = 1;
	struct ip_mreq mreq;

	nullsock = socket(AF_INET, SOCK_DGRAM, 0);
	if (nullsock < 0)
		return -1;

	/* SO_REUSEADDR allows more than one binding to the same
	   socket - you cannot have more than one server on one
	   machine without this */
	if (setsockopt(nullsock, SOL_SOCKET, SO_REUSEADDR, &reuse,
		   sizeof (reuse)) < 0)
		return -1;

	nulladdr.sin_family = AF_INET;
	nulladdr.sin_addr.s_addr = htonl(INADDR_ANY);
	nulladdr.sin_port = htons(port);
	if (bind(nullsock, (struct sockaddr *)&nulladdr,
		 sizeof(nulladdr)) < 0)
		return -1;

	/* Multicast TTL:
	   0 restricted to the same host
	   1 restricted to the same subnet
	   32 restricted to the same site
	   64 restricted to the same region
	   128 restricted to the same continent
	   255 unrestricted

	   DO NOT use a value > 32. If possible, use a value of 1 when
	   testing.
	*/
	if (setsockopt(nullsock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl,
		   sizeof(ttl)) < 0)
		return -1;

	mreq.imr_multiaddr.s_addr = htonl(REPLFSGROUP);
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	if (setsockopt(nullsock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)
		   &mreq, sizeof(mreq)) < 0)
		return -1;

	nulladdr.sin_addr.s_addr = htonl(REPLFSGROUP);
	*sock = nullsock;
	memcpy(addr, &nulladdr, sizeof(nulladdr));

	return 0;
}
