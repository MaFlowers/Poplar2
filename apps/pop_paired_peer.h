#ifndef __POP_PAIRED_SOCK_H__
#define __POP_PAIRED_SOCK_H__
#include "pop_stream.h"
#define MAX_RECV_PACKET_SIZE		102400

/*�����ӵ�TCP�Զ�����*/
enum Tcp_Peer_Type
{
	pop_tcp_peer_type_unknown,		/*δ֪����*/
	pop_tcp_peer_type_server,		/*�Զ���WEB������*/
	pop_tcp_peer_type_client		/*�Զ����û��ͻ���*/
};

/*�������ͣ���ʼΪ��δ֪������֪����������֮��Ϊ�丳ֵ*/
enum Pop_Service_Type
{
	pop_service_type_unknown,
	pop_service_type_http_proxy,	/*HTTP�������*/
	pop_service_type_sock5,			/*SOCK5�������*/
	pop_service_type_dns_client		/*���������ͻ���: ������ֻ����HTTP�����������ʱʹ��*/
};

#define POP_HTTP_CONNECT_TUNNEL		1

/*ͨ�ŶԶ˵���Ϣ*/
struct peer{
	int sockfd;								/*��ʼֵΪ-1*/
	enum Pop_Service_Type service_type;		/*�������ͣ���ҵ������*/

	struct sockaddr_in remoteAddr;			/*�����ֽ���*/
	char *remote_hostname;					/*�Զ˵�������*/
	unsigned int remote_hostip;				/*�Զ˵�ip*/
	unsigned short remote_port;				/*�Զ˶˿ں�*/

	struct sockaddr_in localAddr;
	char *local_hostname;					/*����IP*/
	unsigned int local_hostip;				/*����IP*/
	unsigned short local_port;				/*���ض˿�*/

	/*
		�Զ�������TCP״̬�Ĺ�ϵ��
		tcp_peer_type��Ϊpop_tcp_peer_type_unknown����TCP״̬��Ч
		tcp_peer_type��Ϊpop_tcp_peer_type_server����TCP״̬�ο�tcp_client_status
		tcp_peer_type��Ϊpop_tcp_peer_type_client����TCP״̬�ο�tcp_client_status
	*/
	enum Tcp_Peer_Type tcp_peer_type;		/*�Զ˵�����*/
	int tcp_client_status;					/*����TCP�ͻ���״̬*/
	int tcp_server_status;					/*����TCP�����״̬*/

	/*������Ϣ����ʱ����ҵ�����ͽ���������*/
	struct list *tcp_client_info_list;		/*���˿ͻ�����Ϣ����*/
	struct list *tcp_server_info_list;		/*���˷������Ϣ����*/
	struct event_node *t_info_deal;

	struct peer *paired_peer;				/*��֮�������׽��֣�����ת������һ��,�ýṹ��Ϊ��*/

	struct event_node *t_event;				/*�׽��ֶ�д���¼�*/
	struct event_node *t_connecting_timer;	/*���ӳ�ʱ����*/
	/*�ö�ʱ�¼���Ϊ��������*/
	struct event_node *t_holdtime;			/*���ʱ�¼�����������֧�����õ�TCP���ӣ�����ʱ��û�б��Ľ�������Ҫ�ͷ�����*/

	struct event_node *u_event;				/*UDP�׽��ֶ�д���¼�*/


	/*���ջ���*/
	struct stream *recv_buffer;

	/*���ķ��Ͷ���*/
	struct stream_fifo *write_fifo;
	struct stream_fifo *udp_fifo;/*���������ͻ���ʹ��UDPͨ������������������*/

	/*�Ƿ���WEB���*/
	int if_tunnel;

	/*����ʽ�����������*/
	struct addrinfo *addr_res;
	struct addrinfo *addr_ptr;

	/*DNS�����ʶ*/
	unsigned short DnsTransactionID;
	struct event_node *t_DnsWaitAck;/*�ȴ�DNS��Ӧ��ʱ��*/

	/*
		HTTP�������
		��¼ÿһ��http�������ʼ�У�����TCP��֤�˱���������, �յ���ÿһ����Ӧ����Ϊ�Ƕ����е�һ������Ļ�Ӧ
	*/
	struct list *http_query_sline;
};

struct peer *pop_peer_new();
void pop_peer_delete(struct peer *p);
/*
	���Զ�PEER��TCPͨ���Ƿ����
	�����򷵻�1���������򷵻�0
*/
int pop_check_whether_msg_forward(struct peer *peer);
void pop_peers_paired(struct peer *p1, struct peer *p2);
void pop_tcp_cancle_paired(struct peer *peer);

#endif
