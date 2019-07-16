#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <time.h>
#include "pop_domain_parse.h"
#include "pop_tcp_fsm.h"
#include "pop_socket.h"
#include "memory.h"
#include "popd.h"
#include "pop_comm_hdr.h"
#include "pop_network.h"
#include "pop_domain_cache.h"

unsigned short DomainTransID = 0;

/*��ʼ��DNS����PEER*/
struct peer *pop_domain_parse_peer_init(char *dnsSerAddr, int dsnSerPort)
{
	struct peer *peer = pop_peer_new();
	peer->service_type = pop_service_type_dns_client;	/*DNS CLIENT*/
	peer->tcp_peer_type = pop_tcp_peer_type_server;		/*peer is server*/
	peer->tcp_client_status = Pop_Tcp_Client_Idle;	

	peer->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(peer->sockfd < 0)
	{
		fprintf(stderr, "domain parse sock init failed.\n");
		return NULL;
	}
	sockopt_reuseaddr(peer->sockfd);
	sockopt_reuseport(peer->sockfd);	

	if(dnsSerAddr)
	{
		if(inet_aton(dnsSerAddr, &peer->remoteAddr.sin_addr) < 1)
		{
			fprintf(stderr, "inet_aton error: can't parse the address(%s) :%s\n", dnsSerAddr, 
						safe_strerror(errno));
			
			pop_peer_delete(peer);
			return NULL;
		}
		fprintf(stdout, "Dns parse server: %s\n", dnsSerAddr);
	}
	else
	{
		/*��û��ָ��DNS�����IP����Ĭ��ʹ��8.8.8.8*/
		if(inet_aton(POP_DEFAULT_DNS_SERVER_ADDRESS, &peer->remoteAddr.sin_addr) < 1)
		{
			fprintf(stderr, "inet_aton error: can't parse the address(%s) :%s\n", POP_DEFAULT_DNS_SERVER_ADDRESS, 
						safe_strerror(errno));
			
			pop_peer_delete(peer);
			return NULL;
		}
		fprintf(stdout, "Dns parse server: %s\n", POP_DEFAULT_DNS_SERVER_ADDRESS);	
	}

	peer->remoteAddr.sin_port = htons(dsnSerPort);
	peer->remoteAddr.sin_family = AF_INET;
	
	peer->remote_hostip = ntohl(peer->remoteAddr.sin_addr.s_addr);
	peer->remote_hostname = XSTRDUP(MTYPE_HOSTNAME_STRING, dnsSerAddr);;
	peer->remote_port = dsnSerPort;

	/*һֱ������*/
	EVENT_SET(peer->u_event, pop_udp_read, peer, EVENT_READ, peer->sockfd);

	return peer;
}

/*
	����ֵ:
		1    ---- ��IPV4��ַ
		0/-1 ---- ����IPV4��ַ
*/
int if_ipv4address(char *domainName)
{
	struct sockaddr_in sin;
	assert(domainName);
	size_t domainNameLen = strlen(domainName);
	if(domainNameLen < 7 || domainNameLen > 15)
		return -1;

	return inet_pton(AF_INET, domainName, &sin.sin_addr);
}


/*
	���ɲ�ѯ��
	����ֵ��Ҫ�������ͷ�
*/
static char *_trans_dns_name_into_pkt_format(char *domainName, int *out_len)
{
	char *queryName = NULL, *ptr = NULL, *mptr = NULL;/*mptrָ�����λ��*/
	size_t domainNameLen;
	int count, i;
	if(NULL == domainName)
	{
		fprintf(stderr, "_get_query_name failed: domainName is null.\n");
		return NULL;
	}

	domainNameLen = strlen(domainName);

	/*
		www.baidu.com
		---->0x00www.baidu.com0x00
		---->0x03www0x05baidu0x03com0x00
	*/	
	queryName = XCALLOC(MTYPE_STRING, domainNameLen+2);
	memcpy(queryName + 1, domainName, domainNameLen);
	
	*out_len = domainNameLen+2;
	
	ptr = queryName + 1;
	mptr = queryName;

	count = 0;

	for(i = 0; i < domainNameLen+1; i++)
	{
		if(*ptr != '.' && *ptr != '\0')
		{
			count++;
		}
		else
		{
			*mptr = count;
			mptr = ptr;
			count = 0;
		}

		ptr++;
	}

	return queryName;
}

/*
	e.g. www.baidu.com
	---->0x03www0x05baidu0x03com0x00

	@buffer: pkt��ʽ�������ַ���
	@len: �����ַ�����ʵ�ʳ���
*/
static int check_dns_name_pkt_format(const char *buffer, int len)
{
	const char *ptr = buffer;
	/*
		e.g. www.baidu.com
		---->0x03www0x05baidu0x03com0x00
	*/		
	while(ptr < buffer + len)
		ptr += (unsigned int)(*ptr) + 1;

	if(ptr == buffer + len)
		return 1;/*�Ϸ�*/
	else
		return 0;/*���Ϸ�*/
}

static char *_dns_pkt_name_get(struct stream *s, int *domainNameLen)
{
	char *buff = NULL;
	unsigned short offsetName;
	int domainNamePktLen;
	assert(NULL != s && NULL != domainNameLen);
	
	offsetName = ntohs(*((unsigned short *)STREAM_PNT(s)));
	printf("offsetName: %04x\n", offsetName);
	if((offsetName & DOMAIN_NAME_OFFSET_FLAG) == DOMAIN_NAME_OFFSET_FLAG)
	{/*ƫ������*/
		domainNamePktLen = sizeof(offsetName);
		do{
			UNSET_FLAG(offsetName, DOMAIN_NAME_OFFSET_FLAG);
			fprintf(stdout, "real offset: %04x\n", offsetName);
			buff = (char *)(STREAM_DATA(s) + offsetName);

			offsetName = ntohs(*((unsigned short *)buff));
		}while((offsetName & DOMAIN_NAME_OFFSET_FLAG) == DOMAIN_NAME_OFFSET_FLAG);
	}
	else
	{/*��ʵ����*/
		buff = (char *)STREAM_PNT(s);
		domainNamePktLen = strlen(buff)+1;
	}

	*domainNameLen = domainNamePktLen;
	return buff;
}

/*
	e.g. 0x03www0x05baidu0x03com0x00
	---->www.baidu.com
	@s: ��ʾ�����Ŀ�ʼλ��
	@outQueryName:�������
����ֵ:
	0��������������������Ϊƫ��
	1����������ʵ����
   -1��������ʧ��
*/	
static int _dns_pkt_name_parse(struct stream *s, char **outQueryName)
{
	assert(s != NULL);
	char *buff = NULL;
	char *queryName, *queryNamePtr;
	int len, count, check = 0;
	int domainNamePktLen = 0;
	unsigned short offsetName;
	//debug_show_xbyte(STREAM_PNT(s) ,STREAM_READABLE(s));
	
#if 0
	buff = _dns_pkt_name_get(s, &domainNamePktLen);
#else
	/*����ǰ���������λ��11����˵��������ռ�������ֽڣ���ʾ������ƫ��λ��*/
//DNS��Ӧ�����У�����������ƫ��ֵ��ʾ,ƫ��ֵռ�������ֽڣ��������λ��11��
//���ƫ��ֵ���Զ�����ʾһ��������Ҳ���Խ�������Ĳ���һ���ʾһ������
//����:0xc00c��ʾ�����ڱ�������ƫ��0x000c���ֽڳ�;0x03www0x05baidu0xc011����֪0xc011��ʾ0x03com0x00��
//����0x03www0x05baiduc011��ʾ0x03www0x05baidu0x03com0x00��ת���ɵ�www.baidu.com
	offsetName = stream_getw_from(s, stream_get_getp(s));
	printf("offsetName: %04x\n", offsetName);
	if((offsetName & DOMAIN_NAME_OFFSET_FLAG) == DOMAIN_NAME_OFFSET_FLAG)
	{/*������ƫ��λ��*/
		UNSET_FLAG(offsetName, DOMAIN_NAME_OFFSET_FLAG);
		fprintf(stdout, "real offset: %04x\n", offsetName);
		buff = (char *)(STREAM_DATA(s) + offsetName);

		len = strlen(buff);
		domainNamePktLen = sizeof(offsetName);
		stream_forward_getp(s, domainNamePktLen);
		return 0;/*��ʾ����Ϊƫ��*/
	}
	else
	{/*��ʵ����*/
		buff = (char *)STREAM_PNT(s);

		len = strlen(buff);
		domainNamePktLen = len+1;/*'\0'Ҳ������������*/
	}
#endif
	/*
		e.g. 0x03www0x05baidu0x03com0x00
		---->www.baidu.com
	*/	
	len = strlen(buff);
	check = check_dns_name_pkt_format((const char *)buff, len);
	if(!check)/*���Ϸ�*/
	{
		fprintf(stderr, "illegal dns name pkt format.\n");
		return -1;/*�������Ϸ�*/
	}

	count = *buff;/*��ȡ��һ������*/
	queryName = XCALLOC(MTYPE_STRING, len);
	memcpy(queryName, buff + 1, len);
	queryNamePtr = queryName;

	//debug_show_xbyte(queryName, len);
	
	len = strlen(queryName);
	while(queryNamePtr < queryName + len)
	{
		queryNamePtr += count;
		count = *queryNamePtr;
		if(*queryNamePtr != '\0')
			*queryNamePtr = '.';
		else
			break;
		queryNamePtr++;
	}
	//debug_show_xbyte(queryName, len+1);

	stream_forward_getp(s, domainNamePktLen);
	*outQueryName = queryName;
	
	return 1;/*��ʵ����*/
}


/*��������������*/
int get_host_by_name_unblock(char *domainName)
{
	int ret;
	/*��ѯ��*/
	char *QueryName = NULL;
	int QueryNameLen;
	/*������������ͨ��ͷ��*/
	DomainNameHeader header;

	struct stream *s = NULL;
	
	if(NULL == domainName)
	{
		fprintf(stderr, "Domain parse failed: domainName is null.\n");
		return -1;
	}

	QueryName = _trans_dns_name_into_pkt_format(domainName, &QueryNameLen);
	if(NULL == QueryName)
		return -1;

	memset(&header, 0, sizeof(header));
	
	/*����ͷ��,�������ֶ�header�����ֶ�Ϊ0*/
	header.TransactionID = htons(DomainTransID);/*ʹ��ȫ�ֱ�ʶ��Ȼ���ʶ��1*/
	DomainTransID++;
	header.Flags = htons(POP_DOMAIN_STANDARD_QUERY);/*������׼��ѯ*/
	header.Questions = htons(1);
	
	s = stream_new(sizeof(header) + QueryNameLen + 4);
/*�����ֽ���*/
	/*���ͷ��*/
	stream_put(s, &header, sizeof(header));
	/*����ѯ��*/
	stream_put(s, QueryName, QueryNameLen);
	/*����ѯ���ͺͲ�ѯ��*/
	stream_putw(s, POP_DOMAIN_QUERY_TYPE_A);
	stream_putw(s, POP_DOMAIN_QUERY_CLASS_INTERNET_ADDR);

	ret = sendto(popd->http_proxy_conf.dns_client_peer->sockfd, STREAM_DATA(s), stream_get_endp(s), 0,
					(struct sockaddr *) & popd->http_proxy_conf.dns_client_peer->remoteAddr,
					sizeof (struct sockaddr_in));
	if (ret < 0)
	{
		fprintf(stderr, "can't send packet : (%d)%s", errno, safe_strerror (errno) );
		goto ERROR;
	}

	stream_free(s);
	XFREE(MTYPE_STRING, QueryName);
	fprintf(stdout, "DNS Query, domainName is %s\n", domainName);
	return 0;
ERROR:
	if(QueryName)
		XFREE(MTYPE_STRING, QueryName);
	if(s)
		stream_free(s);
	return -1;
}

void DomainNameQuestionFree(void *p)
{
	DomainNameQuestion *Q = (DomainNameQuestion *)p;
	if(Q->queryName)
		XFREE(MTYPE_STRING, Q->queryName);
	
	XFREE(MTYPE_DNS_QUERY, Q);
}

void DomainNameAnswerFree(void *p)
{
	DomainNameAnswer *A = (DomainNameAnswer *)p;
	if(A->queryName)
		XFREE(MTYPE_STRING, A->queryName);

	XFREE(MTYPE_DNS_ANSWER, A);
}

static DomainNameQuestion *DomainNameQuestionParse(struct stream *s)
{
	int ret;
	DomainNameQuestion *Question = XCALLOC(MTYPE_DNS_QUERY, sizeof(DomainNameQuestion));
	
	ret = _dns_pkt_name_parse(s, &Question->queryName);
	if(ret <= 0)//ֻ��һ�����⣬���Բ�������ƫ��
	{
		fprintf(stderr, "DNS PKT parse error.\n");
		goto ERROR;
	}
	fprintf(stdout, "Question name: %s\n", Question->queryName);
	Question->queryType = stream_getw(s);
	Question->queryClass = stream_getw(s);
	return Question;
	
ERROR:
	XFREE(MTYPE_DNS_QUERY, Question);
	return NULL;
}

static DomainNameAnswer *DomainNameAnswerParse(struct stream *s)
{	
	int ret;
	DomainNameAnswer *Answer = XCALLOC(MTYPE_DNS_ANSWER, sizeof(DomainNameAnswer));
		
	ret = _dns_pkt_name_parse(s, &Answer->queryName);
	if(ret < 0)//ֻ�ܽ�����IP������û��������Ҳû��ϵ
	{
		fprintf(stderr, "DNS PKT parse error.\n");
		goto ERROR;
	}
	fprintf(stdout, "answer name: %s\n", Answer->queryName);
	Answer->Type = stream_getw(s);
	Answer->Class = stream_getw(s);
	Answer->TimeLive = stream_getl(s);
	Answer->Datalen = stream_getw(s);
	/*ֻ����IP��ַ*/
	if(Answer->Type == POP_DOMAIN_QUERY_TYPE_A)
		Answer->ADDR = stream_getl(s);
	else
		stream_forward_getp(s, Answer->Datalen);

	return Answer;
ERROR:
	XFREE(MTYPE_DNS_ANSWER, Answer);
	return NULL;	
}

/*��DNSӦ�����н�����IPV4��ַ������ֵΪNULL��ʾ��ȡʧ��*/
DnsQueryResult *DomainNameIpv4Addr(struct stream *s)
{
	int i;
	DomainNameHeader header;/*����ͷ��*/
	DomainNameQuestion *Question = NULL;/*���ⲿ��*/
	DomainNameAnswer **Answer = NULL;/*��Ӧ����*/
	DnsQueryResult *Result = NULL;
	
/*����Ƿ��ǺϷ���DNS��Ӧ����*/
	if(STREAM_READABLE(s) < sizeof(DomainNameHeader))
	{
		fprintf(stderr, "error format of dns response packet: pkt length.\n");
		goto ERROR;
	}

	header.TransactionID = stream_getw(s);
	header.Flags = stream_getw(s);
	header.Questions = stream_getw(s);
	header.AnswerRRS = stream_getw(s);
	header.AuthorityRRS = stream_getw(s);
	header.AdditionalRRS = stream_getw(s);
	
	if(!(header.Flags & DOMAIN_HEADER_RESPONSE_FLAG))
	{/*����DNS��Ӧ����*/
		fprintf(stderr, "not response dns packet.\n");
		goto ERROR;
	}
	
	fprintf(stdout, "domain query nums: %d, domain answer nums: %d\n", header.Questions, header.AnswerRRS);

	/*�Լ�����ģģΣ�����ֻ��һ�����⣬����Ӧ����ҲӦֻ��һ������*/
	if(header.Questions != 1)
	{
		fprintf(stderr, "question nums is not 1.\n");
		goto ERROR;
	}
	
	/*�Ƚ���DNS���⣬��ʽ:Name + Type + Class*/
	Question = DomainNameQuestionParse(s);
	if(NULL == Question)
		goto ERROR;
	/*������DNS���ⲿ�ֵ�Name��Ϊ��*/
	if(Question->queryType != POP_DOMAIN_QUERY_TYPE_A 
		|| Question->queryClass != POP_DOMAIN_QUERY_CLASS_INTERNET_ADDR
		|| NULL == Question->queryName)
	{
		fprintf(stderr, "error format of dns response packet: unexpected query Type(%d) or Class(%d) or null Name\n",
			Question->queryType, Question->queryClass);
		goto ERROR;
	}

	
	Result = DnsQueryResultNew();
	Result->TransactionID = header.TransactionID;
	Result->HostName = XSTRDUP(MTYPE_STRING, Question->queryName);

/*�ߵ����˵��DNS��Ӧ������*/
	if(header.AnswerRRS <= 0)
	{
		fprintf(stderr, "no answer.\n");
		
		goto DONE;
	}
		
	/*�ٽ���DNS��Ӧ*/
	Answer = XCALLOC(MTYPE_DNS_ANSWER, sizeof(DomainNameAnswer*) * header.AnswerRRS);
	
	for(i = 0; i < header.AnswerRRS; i++)
	{
		Answer[i] = DomainNameAnswerParse(s);
		if(NULL == Answer[i])
			goto DONE;

		if(Answer[i]->Type == POP_DOMAIN_QUERY_TYPE_A)	
			break;
	}
	
	if(Answer[i]->Type != POP_DOMAIN_QUERY_TYPE_A)
		goto DONE;

	Result->TimeLive = Answer[i]->TimeLive;
	Result->Address = Answer[i]->ADDR;
	Result->ValidityEndTime = time(NULL) + Result->TimeLive;


/*��ֱ������DONE���򷵻�ֵResultû����Ӧ��Ϣ(TimeLive��Address)*/	
DONE:
	DomainNameQuestionFree(Question);
	if(Answer)
	{	
		for(i = 0; i < header.AnswerRRS; i++){
			if(Answer[i])
				DomainNameAnswerFree(Answer[i]);
		}
		XFREE(MTYPE_DNS_ANSWER, Answer);
	}

	return Result;

ERROR:
	if(Question)
		DomainNameQuestionFree(Question);
	
	if(Answer)
	{
		for(i = 0; i < header.AnswerRRS; i++){
			if(Answer[i])
				DomainNameAnswerFree(Answer[i]);
		}
		XFREE(MTYPE_DNS_ANSWER, Answer);
	}

	if(Result)
		DnsQueryResultFree(Result);
	
	return NULL;/**/
}

int pop_udp_recv_dns_query_ack(struct peer *peer, struct sockaddr_in from)
{
	DnsQueryResult *dns_result = NULL, *dns_result_tmp = NULL;
	struct peer *server_peer = NULL;
	fprintf(stdout, "udp recv dns query ack from peer %s:%d.\n", ip2str(ntohl(from.sin_addr.s_addr)), ntohs(from.sin_port));
	/*��鱨����Դ�Ƿ���ȷ*/
	if(from.sin_addr.s_addr != peer->remoteAddr.sin_addr.s_addr || from.sin_port != peer->remoteAddr.sin_port)
	{
		/*������������*/
		fprintf(stderr, "unexpect pkt is from %s:%d, expect pkt should be from %s,%d\n", ip2str(ntohl(from.sin_addr.s_addr)), ntohs(from.sin_port),
					ip2str(ntohl(peer->remoteAddr.sin_addr.s_addr)), ntohs(peer->remoteAddr.sin_port));
		goto done;
	}	
	
	dns_result = DomainNameIpv4Addr(peer->recv_buffer);
	if(NULL == dns_result)
		goto done;

	fprintf(stdout, "DNS ANSWER, queryName: %s TransactionID:%d, Address: %s\n", dns_result->HostName, dns_result->TransactionID, ip2str(dns_result->Address));

	server_peer = DnsWaitAckQueueLookupByID(popd->http_proxy_conf.dns_ack_wait_queue, dns_result->TransactionID);
	if(!server_peer)
	{
		fprintf(stderr, "Can't lookup peer by TransactionID.\n");
		goto done;
	}	

	DnsWaitAckQueueDel(popd->http_proxy_conf.dns_ack_wait_queue, server_peer);

	DnsWaitAckTimerCancel(server_peer);

	if(!dns_result->Address)/*DNS��Ӧ������û��Ӧ��*/
		goto done;
	
	server_peer->remoteAddr.sin_addr.s_addr = htonl(dns_result->Address);
	server_peer->remote_hostip = dns_result->Address;
	//server_peer->remote_hostname = XSTRDUP(MTYPE_HOSTNAME_STRING, ip2str(dns_result->Address));

	/*start to establish tcp link*/
	pop_tcp_client_event(server_peer, pop_tcp_client_event_start);

	/*���DNS����*/
	dns_result_tmp = DnsCacheLookupByHostname(popd->http_proxy_conf.dns_cache, dns_result->HostName);
	if(dns_result_tmp)
	{
		dns_result_tmp->ValidityEndTime = dns_result->ValidityEndTime;
		goto done;/*�����Ѵ���*/
	}
	
	/*��DNS���泬����ֵ��ɾ��ԭ�е�1/3���ʶȽϵ͵�DNS����*/
	DnsCacheCheckAndDelete(popd->http_proxy_conf.dns_cache);

	/*��ӻ���*/
	DnsCacheAdd(popd->http_proxy_conf.dns_cache, dns_result);

	return 0;

done:
	if(dns_result)
		DnsQueryResultFree(dns_result);
	return 0;
}

void pop_udp_read(struct event_node *event)
{
	int nbytes;
	struct peer *peer = EVENT_ARG(event);
	struct sockaddr_in from;
	socklen_t fromlen = sizeof(from);

	memset(STREAM_DATA(peer->recv_buffer), 0, STREAM_SIZE(peer->recv_buffer));
	stream_reset(peer->recv_buffer);

	/*������recvfrom*/
	nbytes = stream_recvfrom(peer->recv_buffer, peer->sockfd, STREAM_SIZE(peer->recv_buffer), 
			MSG_DONTWAIT, (struct sockaddr *)&from, &fromlen);
	if(nbytes == -1)//Error: was it fatal
	{
		fprintf(stderr, "recvfrom packet error: (%d)%s\n", errno, safe_strerror(errno));
		goto done;
	}
	else if(nbytes == -2)//Error: was it transient
	{
		goto done;
	}

	/*���ݱ����ճɹ�*/
	
	/*����peer�ķ������ͽ��д���*/
	switch(peer->service_type)
	{
		/*���������ͻ���: ֻ����HTTP�����������ʱʹ��*/
		case pop_service_type_dns_client:
			pop_udp_recv_dns_query_ack(peer, from);
			break;

		default:
			fprintf(stderr, "fatal: unknown service type for udp read.\n");
			goto done;
	}
	

done:	
	if(CHECK_FLAG(GET_LISTEN_EVENTS(event), EVENT_ONESHOT))
		EVENT_SET(peer->u_event, pop_udp_read, peer, GET_LISTEN_EVENTS(event), peer->sockfd);
	
	return;
}

void pop_udp_write(struct event_node *event)
{
	int ret;
	struct peer *peer = EVENT_ARG(event);
	struct stream *stream = NULL;

	if(stream_fifo_head(peer->udp_fifo))
	{
		stream = stream_fifo_pop(peer->udp_fifo);
		ret = sendto(peer->sockfd, stream->data, stream->endp, 0,
					(struct sockaddr *) & peer->remoteAddr,
					sizeof (struct sockaddr_in));
		if(ret < 0)
		{
			fprintf(stderr, "can't send packet: (%d) %s\n", errno, safe_strerror (errno));
			stream_free(stream);
			goto done;
		}
		
		stream_free(stream);
	}
	
done:
	if(stream_fifo_head(peer->udp_fifo))
		EVENT_SET(peer->u_event, pop_udp_write, peer, EVENT_WRITE|EVENT_READ|EVENT_ONESHOT, peer->sockfd);
	else
		EVENT_SET(peer->u_event, pop_udp_write, peer, EVENT_READ|EVENT_ONESHOT, peer->sockfd);
	
	return ;
}

/*UDP��д�¼�,��ʱ����*/
void pop_udp_epoll_event(struct event_node *event)
{
	assert(NULL != event);

	if(EVENT_FD_IF_READ(event->events))
	{
		pop_udp_read(event);
	}
	
	if(EVENT_FD_IF_WRITE(event->events))
	{
		pop_udp_write(event);
	}

	if(EVENT_FD_IF_ERROR(event->events))
	{
		pop_udp_read(event);
	}
}

/*DNS�ȴ���Ӧ���нӿ�*/
unsigned int DnsWaitAckQueueKey(void *p)
{
	return ((struct peer *)p)->DnsTransactionID;
}

int DnsWaitAckQueueCmp(const void *p1, const void *p2)
{
	struct peer *peer1 = (struct peer *)p1;
	struct peer *peer2 = (struct peer *)p2;
	if(peer1->DnsTransactionID == peer2->DnsTransactionID)
		return 1;
	else
		return 0;
}

DnsWaitAckQueue *DnsWaitAckQueueCreate()
{
	return hash_create(DnsWaitAckQueueKey, DnsWaitAckQueueCmp);
}

void DnsWaitAckQueueClean(void *Q)
{
	hash_clean((struct hash * )Q, NULL);
}

void DnsWaitAckQueueFree(void *Q)
{
	hash_clean((struct hash * )Q, NULL);
	hash_free((struct hash * )Q);
}

static void *DnsWaitAckQueueAllocFunc(void *p)
{
	return p;
}

void DnsWaitAckQueueAdd(DnsWaitAckQueue *Q, void *p)
{
	hash_get((struct hash *) Q, p, DnsWaitAckQueueAllocFunc);
}

void *DnsWaitAckQueueDel(DnsWaitAckQueue *Q, void *p)
{
	return hash_release((struct hash *)Q, p);
}

struct peer *DnsWaitAckQueueLookupByID(DnsWaitAckQueue *Q, unsigned short TransactionID)
{
	unsigned int key, index;
	struct hash_backet *backet = NULL;
	struct peer *pair = NULL;
	struct hash *hash = (struct hash *)Q;

	key = TransactionID;
	index = key % hash->size;

	for(backet = hash->index[index]; backet != NULL; backet = backet->next)
	{
		if(backet->key == key)
		{
			pair = (struct peer *)backet->data;
			return pair;
		}
	}

	return NULL;
}

/*DNS��Ӧ����*/
void DnsWaitAckTimeoutCall(struct event_node *node)
{
	struct peer *peer = EVENT_ARG(node);
	peer->t_DnsWaitAck = NULL;

	DnsWaitAckQueueDel(popd->http_proxy_conf.dns_ack_wait_queue, peer);
	/*�Ͽ�pair peer�Ե�TCP����*/
	pop_tcp_close_paired_peer(peer);
	/**/
	#warning "���������WEB��Ӧ������������ʱ"
}

/*�ȴ���Ӧ��ʱ��*/
void DnsWaitAckTimerSet(struct peer *peer)
{
	TIMER_ON(peer->t_DnsWaitAck, DnsWaitAckTimeoutCall, peer, DNS_WAIT_ACK_TIMEOUT);
}

void DnsWaitAckTimerCancel(struct peer* peer)
{
	TIMER_OFF(peer->t_DnsWaitAck);
}

