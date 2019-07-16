#include "pop_transaction.h"
#include "memory.h"
#include "memtypes.h"
#include "list.h"
#include "pop_network.h"
/*事务层*/
/*客户端消息NODE*/
struct tcp_client_info_node *pop_tcp_client_info_node_new()
{
	return (struct tcp_client_info_node *)XCALLOC(MTYPE_TCP_CLIENT_INFO_NODE, sizeof(struct tcp_client_info_node));
}

void pop_tcp_client_info_node_free(void *ev_nd)
{
	XFREE(MTYPE_TCP_CLIENT_INFO_NODE, ev_nd);
}

static void pop_tcp_client_event_http_proxy_call(struct peer *peer, enum tcp_client_info info_type)
{
	fprintf(stdout, "<%s, %d> info_type: %d.\n", __FUNCTION__, __LINE__, info_type);
	switch(info_type)
	{
		case TCP_CLIENT_INFO_CONNECTION_SUCCESS:
			if(POP_HTTP_CONNECT_TUNNEL == peer->if_tunnel)
			{
				struct peer *paired_peer = peer->paired_peer;
				if(paired_peer && stream_fifo_head(paired_peer->write_fifo))
				{
					//EVENT_OFF(paired_peer->t_event);
					EVENT_SET(paired_peer->t_event, pop_http_proxy_tcp_epoll_event, paired_peer, EVENT_READ|EVENT_WRITE|EVENT_ONESHOT, paired_peer->sockfd);
				}
			}
			else
				if(stream_fifo_head(peer->write_fifo))/*有报文则发送*/
				{
					//EVENT_OFF(peer->t_event);
					EVENT_SET(peer->t_event, pop_http_proxy_tcp_epoll_event, peer, EVENT_READ|EVENT_WRITE|EVENT_ONESHOT, peer->sockfd);
				}
			break;

		case TCP_CLIENT_INFO_CONNECTION_ERROR:
			pop_peer_delete(peer);
			break;

		case TCP_CLIENT_INFO_CONNECTION_BREAK:
			pop_peer_delete(peer);
			break;
		default:

			break;
	}
}

static void pop_tcp_client_event_sock5_call(struct peer *peer, enum tcp_client_info info_type)
{
	switch(info_type)
	{
		case TCP_CLIENT_INFO_CONNECTION_SUCCESS:

			break;

		case TCP_CLIENT_INFO_CONNECTION_ERROR:

			break;

		case TCP_CLIENT_INFO_CONNECTION_BREAK:

			break;
		default:

			break;
	}
}

static void pop_tcp_client_event_wrong_call(struct peer *peer, enum tcp_client_info info_type)
{
	switch(info_type)
	{
		case TCP_CLIENT_INFO_CONNECTION_SUCCESS:

			break;

		case TCP_CLIENT_INFO_CONNECTION_ERROR:

			break;

		case TCP_CLIENT_INFO_CONNECTION_BREAK:

			break;
		default:

			break;
	}
}

void pop_tcp_client_event_call(struct peer *peer, void *arg)
{
	enum tcp_client_info info_type = *(enum tcp_client_info*) arg;

	fprintf(stdout, "tcp client event call, info type: %d, peer hostname: %s, service type: %d\n",
		info_type, peer->remote_hostname, peer->service_type);

	if(pop_service_type_http_proxy == peer->service_type)
	{
		pop_tcp_client_event_http_proxy_call(peer, info_type);
	}
	else if(pop_service_type_sock5 == peer->service_type)
	{
		pop_tcp_client_event_sock5_call(peer, info_type);
	}
	else
	{/*unknown type*/
		pop_tcp_client_event_wrong_call(peer, info_type);
	}
}

void pop_tcp_client_event_deal(struct event_node *event)
{
	int i;
	struct peer *peer = EVENT_ARG(event);
	struct tcp_client_info_node *ev_nd = NULL;
	peer->t_info_deal = NULL;

	for(i = 0; i < POP_TRANSINFO_EXECUTE_COUNT; i++)
	{
		/*判断该peer是否有效，比如是否在队列中，若不在，则说明peer进入了结束流程*/
		if(NULL == listnode_lookup(popd->http_proxy_conf.paired_peer_tcp_list, peer))
			return ;

		ev_nd = listnode_head(peer->tcp_client_info_list);
		if(NULL == ev_nd)
			return;
		else
			listnode_delete(peer->tcp_client_info_list, ev_nd);
		
		pop_tcp_client_event_call(peer, &(ev_nd->info_type));
		pop_tcp_client_info_node_free(ev_nd);
	}

	if(listnode_head(peer->tcp_client_info_list))
		TIMER_ON(peer->t_info_deal, pop_tcp_client_event_deal, peer, 0);
}

int pop_tcp_client_event_add(struct peer *peer, enum tcp_client_info info_type)
{
	struct tcp_client_info_node *ev_nd = NULL;

	ev_nd = pop_tcp_client_info_node_new();
	if(NULL == ev_nd)
	{
		fprintf(stderr, "Malloc tcp_client_info_node failed.\n");
		return -1;
	}

	ev_nd->info_type = info_type;
	listnode_add(peer->tcp_client_info_list, ev_nd);

	/*延迟处理消息队列，直接加入*/
	TIMER_ON(peer->t_info_deal, pop_tcp_client_event_deal, peer, 0);
	return 0;
}


/*服务端消息NODE*/
struct tcp_server_info_node *pop_tcp_server_info_node_new()
{
	return (struct tcp_server_info_node *)XCALLOC(MTYPE_TCP_SERVER_INFO_NODE, sizeof(struct tcp_server_info_node));
}

void pop_tcp_server_info_node_free(void *ev_nd)
{
	XFREE(MTYPE_TCP_SERVER_INFO_NODE, ev_nd);
}

static void pop_tcp_server_event_http_proxy_call(struct peer *peer, enum tcp_server_info info_type)
{
	switch(info_type)
	{
		case TCP_SERVER_INFO_ACCEPT:
			
			break;

		case TCP_SERVER_INFO_BREAK:
			pop_peer_delete(peer);
			break;

		case TCP_SERVER_INFO_ERROR:
			pop_peer_delete(peer);
			break;
		default:

			break;
	}
}
static void pop_tcp_server_event_sock5_call(struct peer *peer, enum tcp_server_info info_type)
{
	switch(info_type)
	{
		case TCP_SERVER_INFO_ACCEPT:

			break;

		case TCP_SERVER_INFO_BREAK:

			break;

		case TCP_SERVER_INFO_ERROR:

			break;
		default:

			break;

	}
}

static void pop_tcp_server_event_wrong_call(struct peer *peer, enum tcp_server_info info_type)
{
	switch(info_type)
	{
		case TCP_SERVER_INFO_ACCEPT:

			break;

		case TCP_SERVER_INFO_BREAK:

			break;

		case TCP_SERVER_INFO_ERROR:

			break;
		default:

			break;

	}
}

void pop_tcp_server_event_call(struct peer *peer, void *arg)
{
	enum tcp_server_info info_type = *(enum tcp_server_info*) arg;

	fprintf(stdout, "tcp server event call, info type: %d, peer hostname: %s\n",
		info_type, peer->remote_hostname);

	if(pop_service_type_http_proxy == peer->service_type)
	{
		pop_tcp_server_event_http_proxy_call(peer, info_type);
	}
	else if(pop_service_type_sock5 == peer->service_type)
	{
		pop_tcp_server_event_sock5_call(peer, info_type);
	}
	else
	{/*unknown type*/
		pop_tcp_server_event_wrong_call(peer, info_type);
	}
}

void pop_tcp_server_event_deal(struct event_node *event)
{
	int i;
	struct peer *peer = EVENT_ARG(event);
	struct tcp_server_info_node *ev_nd = NULL;
	peer->t_info_deal = NULL;

	for(i = 0; i < POP_TRANSINFO_EXECUTE_COUNT; i++)
	{
		/*判断该peer是否有效，比如是否在队列中，若不在，则说明peer进入了结束流程*/
		if(NULL == listnode_lookup(popd->http_proxy_conf.paired_peer_tcp_list, peer))
			return;

		ev_nd = listnode_head(peer->tcp_server_info_list);
		if(NULL == ev_nd)
			return;
		else
		{
			//fprintf(stdout, "ev_nd:%d\n", ev_nd->info_type);
			listnode_delete(peer->tcp_server_info_list, ev_nd);
		}
		pop_tcp_server_event_call(peer, &(ev_nd->info_type));
		pop_tcp_server_info_node_free(ev_nd);
	}
	

	if(listnode_head(peer->tcp_server_info_list))
		TIMER_ON(peer->t_info_deal, pop_tcp_server_event_deal, peer, 0);
}


int pop_tcp_server_event_add(struct peer *peer, enum tcp_server_info info_type)
{
	struct tcp_server_info_node *ev_nd = NULL;

	ev_nd = pop_tcp_server_info_node_new();
	if(NULL == ev_nd)
	{
		fprintf(stderr, "Malloc tcp_server_info_node failed.\n");
		return -1;
	}

	ev_nd->info_type = info_type;
	listnode_add(peer->tcp_server_info_list, ev_nd);

	/*延迟处理消息队列，通过“epoll定时器”实现*/
	TIMER_ON(peer->t_info_deal, pop_tcp_server_event_deal, peer, 0);

	return 0;
}

