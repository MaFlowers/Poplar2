#ifndef __POP_TCP_FSM_H__
#define __POP_TCP_FSM_H__

#include "pop_socket.h"
#include "event_ctl.h"

/*建立TCP连接的超时时间*/
#define POP_TCP_CLIENT_CONNECT_TIMEOUT	5

/*一次性最多读10次*/
#define POP_PACKET_READ_TIMES			10
/*一次性最多写10个报文*/
#define POP_PACKET_WRITE_TIMES			10

/*TCP Client status*/
#define Pop_Tcp_Client_Idle				1
#define Pop_Tcp_Client_Connect			2
#define Pop_Tcp_Client_Establish		3
#define MAX_POP_TCP_CLIENT_STATUS		4

/*TCP Client events*/
/*start to establish tcp link*/
#define pop_tcp_client_event_start						1 
/*stop tcp link*/
#define pop_tcp_client_event_stop						2 
/*success to establish tcp*/
#define pop_tcp_client_event_conection_success			3 
/*tcp error*/
#define pop_tcp_client_event_conection_error			4 
/*tcp holdtimer expired*/
#define pop_tcp_client_event_holdtimer_expired 			5
#define MAX_POP_TCP_CLIENT_EVENTS						6

/*TCP Server status*/
#define Pop_Tcp_Server_Idle				1
#define Pop_Tcp_Server_Established		2
#define MAX_POP_TCP_SERVER_STATUS		3

/*TCP Server events*/
#define pop_tcp_server_event_accept_success				1
#define pop_tcp_server_event_connection_error			2
#define pop_tcp_server_event_stop						2
#define pop_tcp_server_event_holdtimer_expired			4
#define MAX_POP_TCP_SERVER_EVENTS						5

void pop_tcp_client_event(struct peer *peer, int event);

void pop_http_proxy_tcp_server_accept(struct event_node *event);
int pop_http_proxy_server_peer_connect(int sock, struct sockaddr_in clientAddr);
int _pop_tcp_server_close(struct peer *peer, const char *fun, const int line);
#define pop_tcp_server_close(peer) _pop_tcp_server_close(peer,__FILE__, __LINE__)


#endif
