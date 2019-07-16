#ifndef __POP_PAIRED_SOCK_H__
#define __POP_PAIRED_SOCK_H__
#include "pop_stream.h"
#define MAX_RECV_PACKET_SIZE		102400

/*已连接的TCP对端类型*/
enum Tcp_Peer_Type
{
	pop_tcp_peer_type_unknown,		/*未知类型*/
	pop_tcp_peer_type_server,		/*对端是WEB服务器*/
	pop_tcp_peer_type_client		/*对端是用户客户端*/
};

/*服务类型，初始为“未知”，在知道服务类型之后，为其赋值*/
enum Pop_Service_Type
{
	pop_service_type_unknown,
	pop_service_type_http_proxy,	/*HTTP代理服务*/
	pop_service_type_sock5,			/*SOCK5代理服务*/
	pop_service_type_dns_client		/*域名解析客户端: 该类型只用于HTTP代理解析域名时使用*/
};

#define POP_HTTP_CONNECT_TUNNEL		1

/*通信对端的信息*/
struct peer{
	int sockfd;								/*初始值为-1*/
	enum Pop_Service_Type service_type;		/*服务类型，即业务类型*/

	struct sockaddr_in remoteAddr;			/*网络字节序*/
	char *remote_hostname;					/*对端的主机名*/
	unsigned int remote_hostip;				/*对端的ip*/
	unsigned short remote_port;				/*对端端口号*/

	struct sockaddr_in localAddr;
	char *local_hostname;					/*本端IP*/
	unsigned int local_hostip;				/*本端IP*/
	unsigned short local_port;				/*本地端口*/

	/*
		对端类型与TCP状态的关系：
		tcp_peer_type若为pop_tcp_peer_type_unknown，则TCP状态无效
		tcp_peer_type若为pop_tcp_peer_type_server，则TCP状态参考tcp_client_status
		tcp_peer_type若为pop_tcp_peer_type_client，则TCP状态参考tcp_client_status
	*/
	enum Tcp_Peer_Type tcp_peer_type;		/*对端的类型*/
	int tcp_client_status;					/*本端TCP客户端状态*/
	int tcp_server_status;					/*本端TCP服务端状态*/

	/*处理消息队列时根据业务类型进行区别处理*/
	struct list *tcp_client_info_list;		/*本端客户端消息队列*/
	struct list *tcp_server_info_list;		/*本端服务端消息队列*/
	struct event_node *t_info_deal;

	struct peer *paired_peer;				/*与之关联的套接字，数据转发的另一端,该结构不为空*/

	struct event_node *t_event;				/*套接字读写等事件*/
	struct event_node *t_connecting_timer;	/*连接超时设置*/
	/*该定时事件作为保留功能*/
	struct event_node *t_holdtime;			/*保活超时事件，服务器不支持永久的TCP连接，若长时间没有报文交互，则要释放连接*/

	struct event_node *u_event;				/*UDP套接字读写等事件*/


	/*接收缓存*/
	struct stream *recv_buffer;

	/*报文发送队列*/
	struct stream_fifo *write_fifo;
	struct stream_fifo *udp_fifo;/*域名解析客户端使用UDP通道向域名服务器请求*/

	/*是否建立WEB隧道*/
	int if_tunnel;

	/*阻塞式域名解析结果*/
	struct addrinfo *addr_res;
	struct addrinfo *addr_ptr;

	/*DNS请求标识*/
	unsigned short DnsTransactionID;
	struct event_node *t_DnsWaitAck;/*等待DNS回应定时器*/

	/*
		HTTP请求队列
		记录每一个http请求的起始行，由于TCP保证了报文有序性, 收到的每一个回应都认为是队列中第一个请求的回应
	*/
	struct list *http_query_sline;
};

struct peer *pop_peer_new();
void pop_peer_delete(struct peer *p);
/*
	检查对端PEER的TCP通道是否可用
	可用则返回1，不可用则返回0
*/
int pop_check_whether_msg_forward(struct peer *peer);
void pop_peers_paired(struct peer *p1, struct peer *p2);
void pop_tcp_cancle_paired(struct peer *peer);

#endif
