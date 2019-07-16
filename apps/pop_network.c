#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include "event_ctl.h"
#include "memory.h"
#include "pop_network.h"
#include "pop_comm_hdr.h"
#include "popd.h"
#include "pop_tcp_fsm.h"
#include "pop_http_proxy_request.h"
#include "pop_transpond.h"
#include "pop_httpcache.h"
int set_nonblocking(int fd)
{
	int flags;

	/* According to the Single UNIX Spec, the return value for F_GETFL should
	never be negative. */
	if ((flags = fcntl(fd, F_GETFL)) < 0)
	{
		fprintf(stderr, "fcntl(F_GETFL) failed for fd %d: %s",
				fd, safe_strerror(errno));
		return -1;
	}
	
	if (fcntl(fd, F_SETFL, (flags | O_NONBLOCK)) < 0)
	{
		fprintf(stderr, "fcntl failed setting fd %d non-blocking: %s",
				fd, safe_strerror(errno));
		return -1;
	}
	
	return 0;
}

int pop_http_proxy_tcp_socket(char *address, int port)
{
	int sockfd;
	struct sockaddr_in sin;
	int ret = -1;
	
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0)
	{
		fprintf(stderr, "socket error: %s\n", safe_strerror(errno));
		return sockfd;
	}

	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port)/*POP_HTTP_PROXY_SERVER_DEFAULT_PORT*/;

	if(address)
	{
		ret = inet_aton(address, &sin.sin_addr);
		if(ret < 1)
		{
			fprintf(stderr, "inet_aton error: can't parse the address(%s) :%s\n", address, 
						safe_strerror(errno));
			close(sockfd);
			return ret;
		}
	}
	else
	{
		sin.sin_addr.s_addr = htonl(INADDR_ANY);
	}

	sockopt_reuseaddr(sockfd);
	sockopt_reuseport(sockfd);

	ret = bind(sockfd, (const struct sockaddr *)&sin, sizeof(struct sockaddr_in));
	if(ret < 0)
	{
		fprintf(stderr, "bind error: %s\n", safe_strerror(errno));
		close(sockfd);
		return ret;
	}

	ret = listen(sockfd, POP_SERVER_DEFAULT_LISTEN_NUM);
	if(ret < 0)
	{
		fprintf(stderr, "listen error: %s\n", safe_strerror(errno));
		close(sockfd);
		return ret;
	}

	fprintf(stdout, "Http proxy server is listening(port:%d)...\n", port);

	/*监听该套接字*/
	event_epoll_add(master, pop_http_proxy_tcp_server_accept, NULL, EVENT_READ, sockfd);

	return sockfd;
}

/*
	描述：关闭套接字对
		当关闭一个套接字时，关闭配对的另一个套接字
*/
void pop_tcp_close_paired_peer(struct peer *peer)
{	
	struct peer *paired_peer = NULL;
	if(NULL == peer)
		return;
	
	paired_peer = peer->paired_peer;
	if(paired_peer)
	{
		if(paired_peer->tcp_peer_type == pop_tcp_peer_type_server)
			pop_tcp_client_event(paired_peer, pop_tcp_client_event_stop);
		else
			pop_tcp_server_close(paired_peer);
	}

	if(peer->tcp_peer_type == pop_tcp_peer_type_server)
		pop_tcp_client_event(peer, pop_tcp_client_event_stop);
	else
		pop_tcp_server_close(peer);	
}

/*处理报文*/
int pop_http_proxy_message_deal(struct peer *peer)
{
	enum Tcp_Peer_Type type = peer->tcp_peer_type;
	struct stream *s = NULL;

	
	if(peer->service_type != pop_service_type_http_proxy)
	{	
		fprintf(stderr, "<%s,%d>fatal error!!\n", __FUNCTION__, __LINE__);
		return -1;
	}

	/*对端是客户端*/
	if(pop_tcp_peer_type_client == type)
	{
		if(pop_check_whether_msg_forward(peer))
		{/*可以直接转发报文*/
			s = stream_dup(peer->recv_buffer);
			pop_tcp_packet_transpond(peer->paired_peer,s);
		}
		else
		{
			pop_http_proxy_request_event(peer);
		}
	}
	else if(pop_tcp_peer_type_server == type)
	{
		if(pop_check_whether_msg_forward(peer))
		{/*可以直接转发报文*/
			fprintf(stdout, "FORWARD packet to peer %s\n", peer->paired_peer->remote_hostname);
			s = stream_dup(peer->recv_buffer);
			pop_tcp_packet_transpond(peer->paired_peer,s);
		}
		else
		{
			/*INVALID*/
			
		}
	}
	else
	{/*unknown type*/
		fprintf(stderr, "<%s, %d>Unknown type %d.\n", __FUNCTION__, __LINE__, type);
		return -1;
	}
	
	return 0;
}

void pop_http_proxy_tcp_epoll_event(struct event_node *event)
{
	assert(NULL != event);

	if(EVENT_FD_IF_READ(event->events))
	{
		pop_http_proxy_tcp_read(event);
	}
	
	if(EVENT_FD_IF_WRITE(event->events))
	{
		pop_http_proxy_tcp_write(event);
	}

	if(EVENT_FD_IF_ERROR(event->events))
	{
		pop_http_proxy_tcp_read(event);
	}
}

int _pop_http_proxy_tcp_write(struct peer *peer)
{
	int /*i,*/ val, num;
	int writenum, write_errno;
	struct stream *stream = NULL;
	if(NULL == peer)
		return -1;
	
	//for(i = 0; i < POP_PACKET_WRITE_TIMES; i++)
	while(1)
	{
		stream = stream_fifo_head(peer->write_fifo);
		if(stream == NULL)
			break;

		/*unblock write*/	
		val = fcntl(peer->sockfd, F_GETFL, 0);
		fcntl(peer->sockfd, F_SETFL, val|O_NONBLOCK);
		writenum = stream_get_endp(stream) - stream_get_getp(stream);
		num = write(peer->sockfd, STREAM_PNT (stream), writenum);
		write_errno = errno;
		fcntl(peer->sockfd, F_SETFL, val);
		if(num < 0)
		{
			if(ERRNO_IO_RETRY(write_errno))
				break;
	
			fprintf(stderr, "Write pkt to peer error, ip: %s  fd:%d: %s\n", 
					peer->remote_hostname, peer->sockfd, safe_strerror(write_errno));

			pop_tcp_close_paired_peer(peer);
			return -1;
		}
	
		/*发送了数据之后，推迟保活定时器*/
		if(num > 0)
		{
			/*可以记录向对端发送了多少字节的包*/			
			fprintf(stdout, "write %d bytes to %s, socket:%d\n", num, peer->remote_hostname, peer->sockfd);
		}
		
		if (num != writenum)
		{
			stream_forward_getp (stream, num);
			break;
		}
	
		stream_free(stream_fifo_pop(peer->write_fifo));
	}

	if(stream_fifo_head(peer->write_fifo))
	{
		//EVENT_OFF(peer->t_event);
		EVENT_SET(peer->t_event, pop_http_proxy_tcp_epoll_event, peer, EVENT_READ|EVENT_ONESHOT|EVENT_WRITE, peer->sockfd);
	}
	else
	{
		//EVENT_OFF(peer->t_event);
		EVENT_SET(peer->t_event, 
					pop_http_proxy_tcp_epoll_event, 
					peer, 
					EVENT_READ|EVENT_ONESHOT,
					peer->sockfd);
	}
	
	return 0;
}

int pop_http_proxy_tcp_write(struct event_node *event)
{
	struct peer *peer = EVENT_ARG(event);
	_pop_http_proxy_tcp_write(peer);
	


	return 0;
}

int pop_http_proxy_tcp_read(struct event_node *event)
{
	int nbytes, i, readerrno;
	struct peer *peer = EVENT_ARG(event);

	for(i = 0; i < POP_PACKET_READ_TIMES; i++)
	{
		memset(STREAM_DATA(peer->recv_buffer), 0, STREAM_SIZE(peer->recv_buffer));
		stream_reset(peer->recv_buffer);
		nbytes = stream_read_unblock(peer->recv_buffer, peer->sockfd, STREAM_SIZE(peer->recv_buffer));
		readerrno = errno;
		if(nbytes < 0)
		{
			if(ERRNO_IO_RETRY(readerrno))
				goto OUT;

			fprintf(stderr, "<%s, %d> [Error] %s read packet error:(%d) %s, fd:%d\n", __FUNCTION__, __LINE__, 
					peer->remote_hostname, readerrno, safe_strerror(readerrno), peer->sockfd);
			
			pop_tcp_close_paired_peer(peer);
			return -1;
		}
		else if(nbytes == 0)
		{
			if(ERRNO_IO_RETRY(readerrno))
				goto OUT;
			fprintf(stderr, "<%s, %d> [Event]tcp connection closed, fd:%d: (%d)%s\n", __FUNCTION__, __LINE__, 
					peer->sockfd, readerrno, safe_strerror(readerrno));
			
			pop_tcp_close_paired_peer(peer);
			return -1;
		}

		fprintf(stdout, "[READ] read bytes :%d from %s\n", nbytes, peer->remote_hostname);
		#warning "如果是长连接, 推迟保活定时器"		
		pop_http_proxy_message_deal(peer);
		
	}

OUT:
	/*再次监听读*/
	EVENT_SET(peer->t_event, pop_http_proxy_tcp_epoll_event, peer, GET_LISTEN_EVENTS(event), peer->sockfd);
	return 0;
}

/*
	先发送报文，若发送缓存满了，在等待下一次的发送
	将报文添加到发送队列

*/
int _pop_tcp_packet_to_write_fifo(struct peer *peer, struct stream *s, const char *func, const int line)
{
	if(NULL == s)
		return -1;

	stream_fifo_push(peer->write_fifo, s);
	_pop_http_proxy_tcp_write(peer);

	return 0;
}

/*
	查找并发送HTTP cache
	若未找到，则返回-1
	发送成功，返回0
*/
//int pop_http_cache

