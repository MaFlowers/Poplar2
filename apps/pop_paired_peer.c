#include "pop_paired_peer.h"
#include "popd.h"
#include "memory.h"
#include "memtypes.h"
#include "pop_tcp_fsm.h"
#include "list.h"
#include "pop_transaction.h"
#include "pop_http_header.h"
struct peer *pop_peer_new()
{
	struct peer *peer = (struct peer *)XCALLOC(MTYPE_PEER, sizeof(struct peer));
	peer->service_type = pop_service_type_unknown;		/*未知服务类型*/
	peer->sockfd = -1;/*初始化为-1*/
	peer->tcp_peer_type = pop_tcp_peer_type_unknown;	/*未知*/
	peer->tcp_client_status = Pop_Tcp_Client_Idle;
	peer->tcp_server_status = Pop_Tcp_Server_Idle;
	peer->tcp_client_info_list = list_new();
	peer->tcp_client_info_list->del = pop_tcp_client_info_node_free;
	peer->tcp_server_info_list = list_new();
	peer->tcp_server_info_list->del = pop_tcp_server_info_node_free;
	peer->http_query_sline = list_new();
	peer->http_query_sline->del = HttpRequestSlineFree;
	/*初始化recv_buffer*/
	peer->recv_buffer = stream_new(MAX_RECV_PACKET_SIZE);
	/*Init writeFifo*/
	peer->write_fifo = stream_fifo_new();
	/*Init udpFifo*/
	peer->udp_fifo = stream_fifo_new();
	
	peer->addr_res = NULL;
	return peer;
}

void pop_peer_delete(struct peer *peer)
{
	assert(peer);
	TIMER_OFF(peer->t_holdtime);
	EVENT_OFF(peer->t_event);
	TIMER_OFF(peer->t_info_deal);
	TIMER_OFF(peer->t_connecting_timer);
	TIMER_OFF(peer->t_DnsWaitAck);
	TIMER_OFF(peer->u_event);
	pop_tcp_cancle_paired(peer);
	
	if(peer->sockfd)
	{
		close(peer->sockfd);
		peer->sockfd = -1;
	}

	listnode_delete(popd->http_proxy_conf.paired_peer_tcp_list, peer);
	DnsWaitAckQueueDel(popd->http_proxy_conf.dns_ack_wait_queue, peer);
	
	if(peer->tcp_client_info_list)
		list_delete(peer->tcp_client_info_list);
	if(peer->tcp_server_info_list)
		list_delete(peer->tcp_server_info_list);
	if(peer->recv_buffer)
		stream_free(peer->recv_buffer);
	if(peer->write_fifo)
		stream_fifo_free(peer->write_fifo);
	if(peer->udp_fifo)
		stream_fifo_free(peer->udp_fifo);
	if(peer->addr_res)
		FREEADDRINFO(peer->addr_res);
	if(peer->remote_hostname)
		XFREE(MTYPE_HOSTNAME_STRING, peer->remote_hostname);
	if(peer->local_hostname)
		XFREE(MTYPE_HOSTNAME_STRING, peer->local_hostname);
	if(peer->http_query_sline)
		list_delete(peer->http_query_sline);
	XFREE(MTYPE_PEER, peer);
}

/*
	检查对端PEER的TCP通道是否可用
	可用则返回1，不可用则返回0
*/
int pop_check_whether_msg_forward(struct peer *peer)
{
	assert(NULL != peer);

	if(NULL == peer->paired_peer)
		return 0;

	if((pop_tcp_peer_type_client == peer->tcp_peer_type)
				&& (pop_tcp_peer_type_server == peer->paired_peer->tcp_peer_type)
				&& (Pop_Tcp_Client_Establish == peer->paired_peer->tcp_client_status))
		return 1;

	if((pop_tcp_peer_type_server == peer->tcp_peer_type)
				&& (pop_tcp_peer_type_client == peer->paired_peer->tcp_peer_type)
				&& (Pop_Tcp_Server_Established == peer->paired_peer->tcp_server_status))
		return 1;

	return 0;
}

void pop_peers_paired(struct peer *p1, struct peer *p2)
{
	p1->paired_peer = p2;
	p2->paired_peer = p1;
}

void pop_tcp_cancle_paired(struct peer *peer)
{
	//return;
	if(peer)
	{
		if(peer->paired_peer)
			peer->paired_peer->paired_peer = NULL;
		peer->paired_peer = NULL;
	}
}

