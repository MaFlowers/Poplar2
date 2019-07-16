#ifndef __POP_DOMAIN_PARSE_H__
#define __POP_DOMAIN_PARSE_H__
#include "pop_paired_peer.h"

extern unsigned short DomainTransID;

#define POP_DEFAULT_DNS_SERVER_ADDRESS	"8.8.8.8"

#define POP_DOMAIN_STANDARD_QUERY					0x0100	/*��׼��ѯ*/
#define POP_DOMAIN_QUERY_TYPE_A						0x0001	/*A���ͣ���ʾ������ȡ��ѯ��ΪIP��ַ*/
#define POP_DOMAIN_QUERY_CLASS_INTERNET_ADDR		0x0001	/*��������ַ*/

#define DOMAIN_HEADER_RESPONSE_FLAG		1<<7

#define DOMAIN_NAME_OFFSET_FLAG			0xc000

/*DNS��Ӧ�ȴ�ʱ��*/
#define DNS_WAIT_ACK_TIMEOUT			3

typedef struct{
	unsigned short TransactionID;/*��ʶ*/
	unsigned short Flags;/*��־*/
	unsigned short Questions;/*������*/
	unsigned short AnswerRRS;/*��Դ��¼��*/
	unsigned short AuthorityRRS;/*��Ȩ��Դ��¼��*/
	unsigned short AdditionalRRS;/*������Դ��¼��*/
}DomainNameHeader;


typedef struct{
	char *queryName;
	unsigned short queryType;
	unsigned short queryClass;
}DomainNameQuestion;

typedef struct{
	char *queryName;
	unsigned short Type;
	unsigned short Class;
	unsigned int TimeLive;
	unsigned short Datalen;
	unsigned int ADDR;
}DomainNameAnswer;

typedef struct hash DnsWaitAckQueue;

struct peer *pop_domain_parse_peer_init(char *dnsSerAddr, int dsnSerPort);
int get_host_by_name_unblock(char *domainName);
void pop_udp_read(struct event_node *event);
int if_ipv4address(char *domainName);


DnsWaitAckQueue *DnsWaitAckQueueCreate();

void DnsWaitAckQueueClean(void *Q);

void DnsWaitAckQueueFree(void *Q);

void DnsWaitAckQueueAdd(DnsWaitAckQueue *Q, void *p);

void *DnsWaitAckQueueDel(DnsWaitAckQueue *Q, void *p);

struct peer *DnsWaitAckQueueLookupByID(DnsWaitAckQueue *Q, unsigned short TransactionID);

void DnsWaitAckTimerSet(struct peer *peer);

void DnsWaitAckTimerCancel(struct peer* peer);

#endif
