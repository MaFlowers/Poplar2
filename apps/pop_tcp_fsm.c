#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "pop_tcp_fsm.h"
#include "event_ctl.h"
#include "pop_paired_peer.h"
#include "memory.h"
#include "memtypes.h"
#include "pop_socket.h"
#include "popd.h"
#include "pop_network.h"
#include "pop_transaction.h"



/*create socket and connect remote server*/
int pop_tcp_client_connect(struct peer *peer)
{
	if(peer->sockfd > 0)
	{
		close(peer->sockfd);
		peer->sockfd = -1;
	}

	peer->sockfd = pop_tcp_socket(peer);
	if(peer->sockfd < 0)
		return CONNECT_ERROR;

	sockopt_reuseaddr(peer->sockfd);
	sockopt_reuseport(peer->sockfd);

	return pop_tcp_connect_unblock(peer);
}

void pop_tcp_client_connect_check(struct event_node *node)
{
	int error;
	socklen_t len;
	struct peer *peer = EVENT_ARG(node);
	fprintf(stdout, "tcp client connect check, ip %s\n", peer->remote_hostname);

	len = sizeof(error);
	if(getsockopt(peer->sockfd, SOL_SOCKET, SO_ERROR, &error, &len) < 0)
	{
		fprintf(stderr, "can't get sockopt for nonblock connect.\n");
		/*连接失败*/
		pop_tcp_close_paired_peer(peer);
		return;
	}
	
	if(error)
	{
		close(peer->sockfd); /*Just in case*/
		fprintf(stderr, "[TCP CLIENT FSM] %s TCP Connect failed (%s) status=%d",
		            peer->remote_hostname, safe_strerror (errno), error);
		pop_tcp_close_paired_peer(peer);
	}
	else
	{
		pop_tcp_client_event(peer, pop_tcp_client_event_conection_success);
	}
}

void pop_tcp_client_connect_expired(struct event_node *event)
{
	struct peer *peer = EVENT_ARG(event);
	peer->t_connecting_timer = NULL;
	fprintf(stdout, "tcp client connect expired, ip %s\n", peer->remote_hostname);

	/*断开TCP连接*/
	pop_tcp_close_paired_peer(peer);
}

int pop_tcp_client_start(struct peer *peer)
{
	int ret = -1;
	ret = pop_tcp_client_connect(peer);
	if(CONNECT_ERROR == ret)
	{
		if(peer->sockfd > 0)
		{
			close(peer->sockfd);
			peer->sockfd = -1;
		}
		
		/*触发事务层TCP_SERVER_INFO_ERROR用于释放*/
		fprintf(stdout, "connect immediatly error.\n");
		pop_tcp_client_event_add(peer, TCP_CLIENT_INFO_CONNECTION_ERROR); 
		return -1;
	}

	EVENT_SET(peer->t_event, 
				pop_tcp_client_connect_check, 
				peer, 
				EVENT_WRITE|EVENT_ONESHOT, 
				peer->sockfd);

	TIMER_ON(peer->t_connecting_timer,
				pop_tcp_client_connect_expired,
				peer,
				POP_TCP_CLIENT_CONNECT_TIMEOUT);
			
	return 0;
}

int pop_tcp_client_ignore(struct peer *peer)
{
	/*ignore*/
	return 0;
}

int pop_tcp_client_establish(struct peer *peer)
{
	TIMER_OFF(peer->t_connecting_timer);
	fprintf(stdout, "tcp client establish, ip %s\n", peer->remote_hostname);

	//pop_paired_peer_hash_add(popd->http_proxy_conf.paired_peer_hash_table, peer, peer->paired_peer);
	/*添加到链表*/
	listnode_add(popd->http_proxy_conf.paired_peer_tcp_list, peer);

	/*通知事务层ESTABLISH，可以转发报文报文了*/
	pop_tcp_client_event_add(peer, TCP_CLIENT_INFO_CONNECTION_SUCCESS);	

	/*监听套接字读*/
	EVENT_SET(peer->t_event, pop_http_proxy_tcp_epoll_event, peer, EVENT_READ|EVENT_ONESHOT, peer->sockfd);

	return 0;
}

int pop_tcp_client_stop(struct peer *peer)
{
	TIMER_OFF(peer->t_holdtime);
	TIMER_OFF(peer->t_connecting_timer);
	EVENT_OFF(peer->t_event);
	fprintf(stdout, "tcp client stop, ip %s\n", peer->remote_hostname);
	
	pop_tcp_cancle_paired(peer);
	if(peer->sockfd)
	{
		close(peer->sockfd);
		peer->sockfd = -1;
	}
	
	if(peer->tcp_client_info_list)
		list_delete_all_node(peer->tcp_client_info_list);
	if(peer->tcp_server_info_list)
		list_delete_all_node(peer->tcp_server_info_list);
	if(peer->recv_buffer)
		stream_reset(peer->recv_buffer);
	if(peer->addr_res)
	{
		FREEADDRINFO(peer->addr_res);
		peer->addr_ptr = NULL;
	}

	/*通知事务层BREAK*/
	pop_tcp_client_event_add(peer, TCP_CLIENT_INFO_CONNECTION_BREAK); 

	return 0;
}

int pop_tcp_client_error(struct peer *peer)
{
	TIMER_OFF(peer->t_holdtime);
	TIMER_OFF(peer->t_connecting_timer);
	EVENT_OFF(peer->t_event);
	fprintf(stdout, "tcp client error, ip %s\n", peer->remote_hostname);
	pop_tcp_cancle_paired(peer);

	if(peer->sockfd)
	{
		close(peer->sockfd);
		peer->sockfd = -1;
	}
	
	if(peer->tcp_client_info_list)
		list_delete_all_node(peer->tcp_client_info_list);
	if(peer->tcp_server_info_list)
		list_delete_all_node(peer->tcp_server_info_list);
	if(peer->recv_buffer)
		stream_reset(peer->recv_buffer);
	if(peer->addr_res)
	{
		FREEADDRINFO(peer->addr_res);
		peer->addr_ptr = NULL;
	}

	/*通知事务层ERROR*/
	pop_tcp_client_event_add(peer, TCP_CLIENT_INFO_CONNECTION_ERROR); 

	return 0;
}

/*TCP客户端状态机*/
struct pop_tcp_client_fsm
{
	int (*func)(struct peer *);
	int next_status;
};

static struct pop_tcp_client_fsm POP_TCP_CLIENT_FSM[MAX_POP_TCP_CLIENT_STATUS-1][MAX_POP_TCP_CLIENT_EVENTS-1] = 
{
	/*Pop_Tcp_Client_Idle*/
	{
		{pop_tcp_client_start,		Pop_Tcp_Client_Connect},/*pop_tcp_client_event_start*/
		{pop_tcp_client_stop/*pop_tcp_client_ignore*/,		Pop_Tcp_Client_Idle},/*pop_tcp_client_event_stop*/
		{pop_tcp_client_ignore,		Pop_Tcp_Client_Idle},/*pop_tcp_client_event_conection_success*/
		{pop_tcp_client_error/*pop_tcp_client_ignore*/,		Pop_Tcp_Client_Idle},/*pop_tcp_client_event_conection_error*/
		{pop_tcp_client_ignore,		Pop_Tcp_Client_Idle},/*pop_tcp_client_event_holdtimer_expired*/
	},

	/*Pop_Tcp_Client_Connect*/
	{
		{pop_tcp_client_ignore,		Pop_Tcp_Client_Connect},/*pop_tcp_client_event_start*/
		{pop_tcp_client_stop,		Pop_Tcp_Client_Idle},/*pop_tcp_client_event_stop*/
		{pop_tcp_client_establish,	Pop_Tcp_Client_Establish},/*pop_tcp_client_event_conection_success*/
		{pop_tcp_client_error,		Pop_Tcp_Client_Idle},/*pop_tcp_client_event_conection_error*/
		{pop_tcp_client_ignore,		Pop_Tcp_Client_Idle},/*pop_tcp_client_event_holdtimer_expired*/
	},

	/*Pop_Tcp_Client_Establish*/
	{
		{pop_tcp_client_ignore,		Pop_Tcp_Client_Establish},/*pop_tcp_client_event_start*/
		{pop_tcp_client_stop,		Pop_Tcp_Client_Idle},/*pop_tcp_client_event_stop*/
		{pop_tcp_client_ignore,		Pop_Tcp_Client_Establish},/*pop_tcp_client_event_conection_success*/
		{pop_tcp_client_error,		Pop_Tcp_Client_Idle},/*pop_tcp_client_event_conection_error*/
		{pop_tcp_client_stop,		Pop_Tcp_Client_Idle},/*pop_tcp_client_event_holdtimer_expired*/
	},	
};

static char *pop_tcp_client_fsm_str[] =
{
    NULL,
    "Pop_Tcp_Client_Idle",
    "Pop_Tcp_Client_Connect",
    "Pop_Tcp_Client_Establish",
};

char *pop_tcp_client_event_str[] =
{
    NULL,
    "pop_tcp_client_event_start",
    "pop_tcp_client_event_stop",
    "pop_tcp_client_event_conection_success",
    "pop_tcp_client_event_conection_error",
    "mcp_tcp_client_keepalive_timer_expired",
    "pop_tcp_client_event_holdtimer_expired",
};

void pop_tcp_client_event(struct peer *peer, int event)
{
	int ret = 0, next;

	/* Logging this event. */
	next = POP_TCP_CLIENT_FSM[peer->tcp_client_status -1][event - 1].next_status;

	if(peer->tcp_client_status != next)
		fprintf(stdout, "%s [TCP CLIENT FSM] %s (%s->%s)\n", peer->remote_hostname,
			pop_tcp_client_event_str[event], pop_tcp_client_fsm_str[peer->tcp_client_status],
			pop_tcp_client_fsm_str[next]);

	/* Call function. */
	if(POP_TCP_CLIENT_FSM[peer->tcp_client_status -1][event - 1].func)
		ret = (* (POP_TCP_CLIENT_FSM[peer->tcp_client_status - 1][event - 1].func) ) (peer);

	/* When function do not want proceed next job return -1. */
	if(ret >= 0)
	{
		/* If status is changed. */
		if(next != peer->tcp_client_status)
			peer->tcp_client_status = next;
	}  
}


/*Server*/
int pop_http_proxy_server_peer_connect(int sock, struct sockaddr_in clientAddr)
{
	struct sockaddr_in localAddr;
	socklen_t addrlen = sizeof(struct sockaddr_in);
	char Ipaddr[128];

	struct peer *peer = pop_peer_new();

	peer->sockfd = sock;
	memset(Ipaddr, 0, sizeof(Ipaddr));
	inet_ntop(AF_INET, &clientAddr.sin_addr, Ipaddr, INET_ADDRSTRLEN/*16*/);
	peer->remote_hostip = ntohl(clientAddr.sin_addr.s_addr);
	peer->remote_hostname = XSTRDUP(MTYPE_HOSTNAME_STRING, Ipaddr);
	peer->remote_port = ntohs(clientAddr.sin_port);
	peer->remoteAddr = clientAddr;
	
	peer->service_type = pop_service_type_http_proxy;	/*HTTP PROXY*/
	peer->tcp_peer_type = pop_tcp_peer_type_client;	/*对端是客户端*/
	peer->tcp_server_status = Pop_Tcp_Server_Established;	/*TCP连接建立成功*/

	fprintf(stdout, "TCP connection from %s:%d\n", peer->remote_hostname, peer->remote_port);
	
	/*获取TCP连接的本端协议地址*/
	if(0 == getsockname(sock, (struct sockaddr *)&localAddr, &addrlen))
	{
		memset(Ipaddr, 0, sizeof(Ipaddr));
		inet_ntop(AF_INET, &localAddr.sin_addr, Ipaddr, INET_ADDRSTRLEN/*16*/);

		peer->local_hostip = ntohl(localAddr.sin_addr.s_addr);
		peer->local_hostname = XSTRDUP(MTYPE_HOSTNAME_STRING, Ipaddr);
		peer->local_port = ntohs(localAddr.sin_port);
		peer->localAddr = localAddr;

		fprintf(stdout, "client connectd server ip %s port %d\n", peer->local_hostname, peer->local_port);
	}

	/*监听套接字读事件*/
	EVENT_SET(peer->t_event, pop_http_proxy_tcp_epoll_event, peer, EVENT_READ|EVENT_ONESHOT, peer->sockfd);

	/*添加到链表*/
	listnode_add(popd->http_proxy_conf.paired_peer_tcp_list, peer);

	/*设置超时定时器*/

	/*TCP连接成功事务层*/
	pop_tcp_server_event_add(peer, TCP_SERVER_INFO_ACCEPT);
	/**/
	return 0;
}

void pop_http_proxy_tcp_server_accept(struct event_node *event)
{
	int sockfd, client_sock;
	
	struct sockaddr_in clientaddr;

	sockfd = EVENT_FD(event);

	if(sockfd < 0)
	{
		fprintf(stderr, "accept error: sock is negative value %d\n", sockfd);
		if(popd->http_proxy_conf.sockfd > 0)
			event_epoll_add(master, pop_http_proxy_tcp_server_accept, NULL, EVENT_READ, popd->http_proxy_conf.sockfd);
		return;
	}

	if(sockfd != popd->http_proxy_conf.sockfd)
	{
		fprintf(stderr, "invalid sockfd(%d), tcp sock is (%d)\n", sockfd, popd->http_proxy_conf.sockfd);
		if(popd->http_proxy_conf.sockfd > 0)
			event_epoll_add(master, pop_http_proxy_tcp_server_accept, NULL, EVENT_READ, popd->http_proxy_conf.sockfd);
		return;
	}

	/*EPOLL的ET边缘模式，一次性将所有连接处理完*/
	while(1)
	{
		/*为防止ACCEPT时,TCP等待队列为空，使用非阻塞的ACCEPT*/
		client_sock = pop_tcp_accept_unblock(sockfd, (struct sockaddr *)&clientaddr);
		if(client_sock < 0)
		{
			if(ERRNO_ACCEPT_RETRY(errno))
			{
				//fprintf(stdout, "accept failed: %d:%s\n", errno, safe_strerror(errno));
				goto OUT;
			}

			/*TCP断开*/
			fprintf(stderr, "accept error!!!!!cancel accept %d\n", sockfd);
			event_cancel(event);/*取消监听套机字读*/
			return;
		}
		
		pop_http_proxy_server_peer_connect(client_sock, clientaddr);
	}

OUT:	
	//event_epoll_add(master, pop_http_proxy_tcp_server_accept, NULL, EVENT_READ|EVENT_ONESHOT, sockfd);
	return;
}

#if 0
static int pop_tcp_read_unblock(int sockfd, char *buffer, size_t len)
{
	int val, nbytes;
	val = fcntl (sockfd, F_GETFL, 0);
	fcntl (sockfd, F_SETFL, val|O_NONBLOCK);
	nbytes = read (sockfd, buffer, len);
	fcntl (sockfd, F_SETFL, val);

	return nbytes;
}
#endif


int _pop_tcp_server_close(struct peer *peer, const char *fun, const int line)
{
	TIMER_OFF(peer->t_holdtime);
	EVENT_OFF(peer->t_event);
	TIMER_OFF(peer->t_connecting_timer);
	TIMER_OFF(peer->t_DnsWaitAck);
	TIMER_OFF(peer->u_event);
	TIMER_OFF(peer->t_info_deal);

	peer->tcp_server_status = Pop_Tcp_Server_Idle;
	
	pop_tcp_cancle_paired(peer);
	if (peer->sockfd > 0)
	{
		close (peer->sockfd);
		peer->sockfd = -1;
	}
	
	fprintf(stdout, "tcp server close client peer:%s  <%s,%d>\n", peer->remote_hostname, fun, line);
	if(peer->tcp_client_info_list)
		list_delete_all_node(peer->tcp_client_info_list);
	if(peer->tcp_server_info_list)
		list_delete_all_node(peer->tcp_server_info_list);
	if(peer->recv_buffer)
		stream_reset(peer->recv_buffer);
	if(peer->addr_res)
	{
		FREEADDRINFO(peer->addr_res);
		peer->addr_ptr = NULL;
	}


	/*通知事务层BREAK，释放PEER*/
	//pop_peer_free(peer);
	pop_tcp_server_event_add(peer, TCP_SERVER_INFO_BREAK);
	return 0;
}


/*TCP服务端状态机*/
int pop_tcp_server_established(struct peer *peer)
{
	struct sockaddr_in localAddr;
	socklen_t addrlen = sizeof(struct sockaddr_in);
	char Ipaddr[128];

	fprintf(stdout, "TCP connection from %s:%d\n", peer->remote_hostname, peer->remote_port);
	
	/*获取TCP连接的本端协议地址*/
	if(0 == getsockname(peer->sockfd, (struct sockaddr *)&localAddr, &addrlen))
	{
		memset(Ipaddr, 0, sizeof(Ipaddr));
		inet_ntop(AF_INET, &localAddr.sin_addr, Ipaddr, INET_ADDRSTRLEN/*16*/);

		peer->local_hostip = ntohl(localAddr.sin_addr.s_addr);
		peer->local_hostname = XSTRDUP(MTYPE_HOSTNAME_STRING, Ipaddr);
		peer->local_port = ntohs(localAddr.sin_port);
		peer->localAddr = localAddr;

		fprintf(stdout, "client connectd server ip %s port %d\n", peer->local_hostname, peer->local_port);
	}

	/*监听套接字读事件*/
	EVENT_SET(peer->t_event, pop_http_proxy_tcp_epoll_event, peer, EVENT_READ|EVENT_ONESHOT, peer->sockfd);

	/*添加到链表*/
	listnode_add(popd->http_proxy_conf.paired_peer_tcp_list, peer);

	/*设置超时定时器*/

	/*TCP连接成功事务层*/
	pop_tcp_server_event_add(peer, TCP_SERVER_INFO_ACCEPT);
	/**/

	return 0;
}

int pop_tcp_server_ignore(struct peer *peer)
{
	return 0;
}

int pop_tcp_server_error(struct peer *peer)
{
	TIMER_OFF(peer->t_holdtime);
	EVENT_OFF(peer->t_event);
	TIMER_OFF(peer->t_connecting_timer);
	
	pop_tcp_cancle_paired(peer);
	if (peer->sockfd > 0)
	{
		close (peer->sockfd);
		peer->sockfd = -1;
	}
	
	fprintf(stdout, "tcp server close client peer:%s\n", peer->remote_hostname);
	if(peer->tcp_client_info_list)
		list_delete_all_node(peer->tcp_client_info_list);
	if(peer->tcp_server_info_list)
		list_delete_all_node(peer->tcp_server_info_list);
	if(peer->recv_buffer)
		stream_reset(peer->recv_buffer);
	if(peer->addr_res)
	{
		FREEADDRINFO(peer->addr_res);
		peer->addr_ptr = NULL;
	}

	/*通知事务层BREAK，释放PEER*/
	pop_tcp_server_event_add(peer, TCP_SERVER_INFO_ERROR);
	return 0;

}

int pop_tcp_server_stop(struct peer *peer)
{
	TIMER_OFF(peer->t_holdtime);
	EVENT_OFF(peer->t_event);
	TIMER_OFF(peer->t_connecting_timer);
	
	pop_tcp_cancle_paired(peer);
	if (peer->sockfd > 0)
	{
		close (peer->sockfd);
		peer->sockfd = -1;
	}
	
	fprintf(stdout, "tcp server close client peer:%s\n", peer->remote_hostname);
	if(peer->tcp_client_info_list)
		list_delete_all_node(peer->tcp_client_info_list);
	if(peer->tcp_server_info_list)
		list_delete_all_node(peer->tcp_server_info_list);
	if(peer->recv_buffer)
		stream_reset(peer->recv_buffer);
	if(peer->addr_res)
	{
		FREEADDRINFO(peer->addr_res);
		peer->addr_ptr = NULL;
	}

	/*通知事务层BREAK，释放PEER*/
	pop_tcp_server_event_add(peer, TCP_SERVER_INFO_BREAK);
	return 0;

}


struct pop_tcp_server_fsm
{
	int (*func)(struct peer *);
	int next_status;
};

static struct pop_tcp_server_fsm POP_TCP_SERVER_FSM[MAX_POP_TCP_SERVER_STATUS-1][MAX_POP_TCP_SERVER_EVENTS-1] = 
{
	/*Pop_Tcp_Server_Idle*/
	{
		{pop_tcp_server_established,		Pop_Tcp_Server_Established},/*pop_tcp_server_event_accept_success*/
		{pop_tcp_server_ignore,				Pop_Tcp_Server_Idle},/*pop_tcp_server_event_connection_error*/
		{pop_tcp_server_ignore,				Pop_Tcp_Server_Idle},/*pop_tcp_server_event_stop*/
		{pop_tcp_server_ignore,				Pop_Tcp_Server_Idle},/*pop_tcp_server_event_holdtimer_expired*/
	},

	/*Pop_Tcp_Server_Established*/
	{
		{pop_tcp_server_ignore,				Pop_Tcp_Server_Established},/*pop_tcp_server_event_accept_success*/
		{pop_tcp_server_error, 				Pop_Tcp_Server_Idle},/*pop_tcp_server_event_connection_error*/
		{pop_tcp_server_stop, 				Pop_Tcp_Server_Idle},/*pop_tcp_server_event_stop*/
		{pop_tcp_server_stop, 				Pop_Tcp_Server_Idle},/*pop_tcp_server_event_holdtimer_expired*/
	},

};

static char *pop_tcp_server_fsm_str[] =
{
    NULL,
    "Pop_Tcp_Server_Idle",
    "Pop_Tcp_Server_Established",
};

char *pop_tcp_server_event_str[] =
{
    NULL,
    "pop_tcp_server_event_accept_success",
    "pop_tcp_server_event_connection_error",
    "pop_tcp_server_event_stop",
    "pop_tcp_server_event_holdtimer_expired",
};

void pop_tcp_server_event(struct peer *peer, int event)
{
	int ret = 0, next;

	/* Logging this event. */
	next = POP_TCP_SERVER_FSM[peer->tcp_server_status -1][event - 1].next_status;

	if(peer->tcp_server_status != next)
		fprintf(stdout, "%s [TCP SERVER FSM] %s (%s->%s)\n", peer->remote_hostname,
			pop_tcp_server_event_str[event], pop_tcp_server_fsm_str[peer->tcp_server_status],
			pop_tcp_server_fsm_str[next]);

	/* Call function. */
	if(POP_TCP_SERVER_FSM[peer->tcp_server_status -1][event - 1].func)
		ret = (* (POP_TCP_SERVER_FSM[peer->tcp_server_status - 1][event - 1].func) ) (peer);

	/* When function do not want proceed next job return -1. */
	if(ret >= 0)
	{
		/* If status is changed. */
		if(next != peer->tcp_server_status)
			peer->tcp_server_status = next;
	}  
}

