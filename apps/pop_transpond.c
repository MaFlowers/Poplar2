#include "pop_transpond.h"
#include "pop_network.h"
int pop_tcp_packet_transpond(struct peer *peer, struct stream *s)
{
	if(NULL == peer)
	{
		fprintf(stderr, "Cannot transpond packet: No paired peer to send packet.\n");
		return -1;
	}
	pop_tcp_packet_to_write_fifo(peer, s);

	return 0;
}

