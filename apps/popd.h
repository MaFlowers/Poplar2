#ifndef __POPD_H__
#define __POPD_H__
#include "event_ctl.h"
#include <fcntl.h>
#include <unistd.h>
#include "pop_paired_peer.h"
#include "pop_paired_peer_hash.h"
#include "hash.h"
#include "list.h"
#include "util.h"
#include "pop_domain_parse.h"
#include "pop_domain_cache.h"

/*EPOLLԤ�Ƽ��100���׽���*/
#define POP_EXPECTED_SIZE	100
/*һ���Դ���20���׽����¼�*/
#define MAXEVENTS			20


struct event_master *master;
//pthread_mutex_t pop_peer_lock;
extern struct pop_gl_config *popd;

/*timer*/
#define TIMER_ON(timer,func,arg,time) EPOLL_TIMER_ADD(master,timer,func,arg,time)
#define TIMER_MSEC_ON(timer,func,arg,time) EPOLL_TIMER_ADD_MSEC(master,timer,func,arg,time)
#define REAR_ON(timer,func,arg,time) EPOLL_REAR_ADD(master,timer,func,arg,time)
#define REAR_MSEC_ON(timer,func,arg,time) EPOLL_REAR_ADD_MSEC(master,timer,func,arg,time)
#define TIMER_OFF(timer) EPOLL_TIMER_OFF(timer)
/*file description*/
#define EVENT_ADD(event,func,arg,ep_events,fd) EPOLL_EVENT_ADD(master,event,func,arg,ep_events,fd)
#define EVENT_MOD(event,func,arg,ep_events,fd) EPOLL_EVENT_MOD(master,event,func,arg,ep_events,fd)

#define EVENT_SET(event,func,arg,ep_events,fd)\
	do{\
		if(event)\
			EPOLL_EVENT_MOD(master,event,func,arg,ep_events,fd);\
		else\
			EPOLL_EVENT_ADD(master,event,func,arg,ep_events,fd);\
	}while(0)

#define EVENT_OFF(event) EPOLL_EVENT_OFF(event)

char * _bytes_to_x(unsigned char *src_buf, int len);

void _debug_show_xbyte(unsigned char *src_buf, int len, const char *func_str, int code_line);
#define debug_show_xbyte(a, b) _debug_show_xbyte(a, b, __FUNCTION__,__LINE__)


/*HTTP���������˽������*/
typedef struct pop_http_proxy_config
{
	int sockfd;								/*���ڼ����û���TCP��������*/
	struct list *paired_peer_tcp_list;		/*���ڴ洢peer*/
	struct hash *paired_peer_hash_table;	/*hash table:�洢�ͻ����Լ������peer, ����ʹ����*/
	
	struct peer *dns_client_peer;/*DNS�����ͻ���*/
	DnsWaitAckQueue *dns_ack_wait_queue;/*DNS����ȴ���Ӧ����*/
	DnsCache *dns_cache;/*Dns����*/

}POP_HTTP_PROXY_CONF;

struct pop_gl_config
{
	POP_HTTP_PROXY_CONF http_proxy_conf;
};

int pop_init();


#endif
