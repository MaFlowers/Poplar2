#ifndef __POP_TRANSACTION_H__
#define __POP_TRANSACTION_H__

#include "popd.h"

/*一次最多处理5个事件*/
#define POP_TRANSINFO_EXECUTE_COUNT		5

/*客户端消息NODE*/
enum tcp_client_info
{
	TCP_CLIENT_INFO_CONNECTION_SUCCESS, 	/*连接成功*/
	TCP_CLIENT_INFO_CONNECTION_ERROR,		/*连接失败*/
	TCP_CLIENT_INFO_CONNECTION_BREAK		/*断开连接*/
};
	
struct tcp_client_info_node
{
	enum tcp_client_info info_type;
};


/*服务端消息NODE*/
enum tcp_server_info
{
	TCP_SERVER_INFO_ACCEPT,					/*连接建立成功*/
	TCP_SERVER_INFO_BREAK,					/*连接断开*/
	TCP_SERVER_INFO_ERROR,					/*连接失败*/
};

struct tcp_server_info_node
{
	enum tcp_server_info info_type;
};


/*消息*/
struct tcp_client_info_node *pop_tcp_client_info_node_new();
void pop_tcp_client_info_node_free(void *ev_nd);
int pop_tcp_client_event_add(struct peer *peer, enum tcp_client_info info_type);
struct tcp_server_info_node *pop_tcp_server_info_node_new();
void pop_tcp_server_info_node_free(void *ev_nd);
int pop_tcp_server_event_add(struct peer *peer, enum tcp_server_info info_type);


#endif
