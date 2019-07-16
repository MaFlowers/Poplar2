#ifndef __POP_TRANSACTION_H__
#define __POP_TRANSACTION_H__

#include "popd.h"

/*һ����ദ��5���¼�*/
#define POP_TRANSINFO_EXECUTE_COUNT		5

/*�ͻ�����ϢNODE*/
enum tcp_client_info
{
	TCP_CLIENT_INFO_CONNECTION_SUCCESS, 	/*���ӳɹ�*/
	TCP_CLIENT_INFO_CONNECTION_ERROR,		/*����ʧ��*/
	TCP_CLIENT_INFO_CONNECTION_BREAK		/*�Ͽ�����*/
};
	
struct tcp_client_info_node
{
	enum tcp_client_info info_type;
};


/*�������ϢNODE*/
enum tcp_server_info
{
	TCP_SERVER_INFO_ACCEPT,					/*���ӽ����ɹ�*/
	TCP_SERVER_INFO_BREAK,					/*���ӶϿ�*/
	TCP_SERVER_INFO_ERROR,					/*����ʧ��*/
};

struct tcp_server_info_node
{
	enum tcp_server_info info_type;
};


/*��Ϣ*/
struct tcp_client_info_node *pop_tcp_client_info_node_new();
void pop_tcp_client_info_node_free(void *ev_nd);
int pop_tcp_client_event_add(struct peer *peer, enum tcp_client_info info_type);
struct tcp_server_info_node *pop_tcp_server_info_node_new();
void pop_tcp_server_info_node_free(void *ev_nd);
int pop_tcp_server_event_add(struct peer *peer, enum tcp_server_info info_type);


#endif
