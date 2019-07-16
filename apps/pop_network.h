#ifndef __POP_NETWORK_H__
#define __POP_NETWORK_H__
#include "pop_paired_peer.h"
extern int set_nonblocking(int fd);

#define ERRNO_IO_RETRY(EN) \
	(((EN) == EAGAIN) || ((EN) == EWOULDBLOCK) || ((EN) == EINTR))

#define ERRNO_ACCEPT_RETRY(EN) \
	((EWOULDBLOCK == (EN)) || (ECONNABORTED == (EN)) || (EPROTO == (EN)) || (EINTR == (EN))) 

/*¼àÌý¶Ë¿Ú*/
#define POP_HTTP_PROXY_SERVER_DEFAULT_PORT		8080
#define POP_SERVER_DEFAULT_LISTEN_NUM			128			

int pop_http_proxy_tcp_socket(char *address, int port);
int pop_http_proxy_message_deal(struct peer *peer);

int pop_http_proxy_tcp_read(struct event_node *event);
int pop_http_proxy_tcp_write(struct event_node *event);
int _pop_tcp_packet_to_write_fifo(struct peer *peer, struct stream *s, const char *func, const int line);
#define pop_tcp_packet_to_write_fifo(a,b) _pop_tcp_packet_to_write_fifo(a,b,__FUNCTION__,__LINE__)
void pop_http_proxy_tcp_epoll_event(struct event_node *event);

void pop_tcp_close_paired_peer(struct peer *peer);

#endif
