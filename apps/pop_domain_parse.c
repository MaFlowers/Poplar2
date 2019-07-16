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

/*初始化DNS解析PEER*/
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
		/*若没有指定DNS服务端IP，则默认使用8.8.8.8*/
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

	/*一直监听读*/
	EVENT_SET(peer->u_event, pop_udp_read, peer, EVENT_READ, peer->sockfd);

	return peer;
}

/*
	返回值:
		1    ---- 是IPV4地址
		0/-1 ---- 不是IPV4地址
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
	生成查询名
	返回值需要调用者释放
*/
static char *_trans_dns_name_into_pkt_format(char *domainName, int *out_len)
{
	char *queryName = NULL, *ptr = NULL, *mptr = NULL;/*mptr指向计数位置*/
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

	@buffer: pkt格式的域名字符串
	@len: 域名字符串的实际长度
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
		return 1;/*合法*/
	else
		return 0;/*不合法*/
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
	{/*偏移域名*/
		domainNamePktLen = sizeof(offsetName);
		do{
			UNSET_FLAG(offsetName, DOMAIN_NAME_OFFSET_FLAG);
			fprintf(stdout, "real offset: %04x\n", offsetName);
			buff = (char *)(STREAM_DATA(s) + offsetName);

			offsetName = ntohs(*((unsigned short *)buff));
		}while((offsetName & DOMAIN_NAME_OFFSET_FLAG) == DOMAIN_NAME_OFFSET_FLAG);
	}
	else
	{/*真实域名*/
		buff = (char *)STREAM_PNT(s);
		domainNamePktLen = strlen(buff)+1;
	}

	*domainNameLen = domainNamePktLen;
	return buff;
}

/*
	e.g. 0x03www0x05baidu0x03com0x00
	---->www.baidu.com
	@s: 表示域名的开始位置
	@outQueryName:输出域名
返回值:
	0——不返回域名，域名为偏移
	1——返回真实域名
   -1——解析失败
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
	/*若最前面的两个高位是11，则说明域名仅占用两个字节，表示域名的偏移位置*/
//DNS响应报文中，域名可能用偏移值表示,偏移值占用两个字节，且最高两位是11；
//这个偏移值可以独立表示一个域名，也可以结合域名的部分一起表示一个域名
//例如:0xc00c表示域名在报文往后偏移0x000c个字节出;0x03www0x05baidu0xc011，已知0xc011表示0x03com0x00，
//所以0x03www0x05baiduc011表示0x03www0x05baidu0x03com0x00，转换可得www.baidu.com
	offsetName = stream_getw_from(s, stream_get_getp(s));
	printf("offsetName: %04x\n", offsetName);
	if((offsetName & DOMAIN_NAME_OFFSET_FLAG) == DOMAIN_NAME_OFFSET_FLAG)
	{/*域名是偏移位置*/
		UNSET_FLAG(offsetName, DOMAIN_NAME_OFFSET_FLAG);
		fprintf(stdout, "real offset: %04x\n", offsetName);
		buff = (char *)(STREAM_DATA(s) + offsetName);

		len = strlen(buff);
		domainNamePktLen = sizeof(offsetName);
		stream_forward_getp(s, domainNamePktLen);
		return 0;/*表示域名为偏移*/
	}
	else
	{/*真实域名*/
		buff = (char *)STREAM_PNT(s);

		len = strlen(buff);
		domainNamePktLen = len+1;/*'\0'也计入域名长度*/
	}
#endif
	/*
		e.g. 0x03www0x05baidu0x03com0x00
		---->www.baidu.com
	*/	
	len = strlen(buff);
	check = check_dns_name_pkt_format((const char *)buff, len);
	if(!check)/*不合法*/
	{
		fprintf(stderr, "illegal dns name pkt format.\n");
		return -1;/*域名不合法*/
	}

	count = *buff;/*获取第一个计数*/
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
	
	return 1;/*真实域名*/
}


/*非阻塞域名解析*/
int get_host_by_name_unblock(char *domainName)
{
	int ret;
	/*查询名*/
	char *QueryName = NULL;
	int QueryNameLen;
	/*域名解析报文通用头部*/
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
	
	/*构造头部,除以下字段header其他字段为0*/
	header.TransactionID = htons(DomainTransID);/*使用全局标识，然后标识加1*/
	DomainTransID++;
	header.Flags = htons(POP_DOMAIN_STANDARD_QUERY);/*域名标准查询*/
	header.Questions = htons(1);
	
	s = stream_new(sizeof(header) + QueryNameLen + 4);
/*网络字节序*/
	/*填充头部*/
	stream_put(s, &header, sizeof(header));
	/*填充查询名*/
	stream_put(s, QueryName, QueryNameLen);
	/*填充查询类型和查询类*/
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
	if(ret <= 0)//只有一个问题，所以不可能是偏移
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
	if(ret < 0)//只管解析出IP，域名没解析出来也没关系
	{
		fprintf(stderr, "DNS PKT parse error.\n");
		goto ERROR;
	}
	fprintf(stdout, "answer name: %s\n", Answer->queryName);
	Answer->Type = stream_getw(s);
	Answer->Class = stream_getw(s);
	Answer->TimeLive = stream_getl(s);
	Answer->Datalen = stream_getw(s);
	/*只关心IP地址*/
	if(Answer->Type == POP_DOMAIN_QUERY_TYPE_A)
		Answer->ADDR = stream_getl(s);
	else
		stream_forward_getp(s, Answer->Datalen);

	return Answer;
ERROR:
	XFREE(MTYPE_DNS_ANSWER, Answer);
	return NULL;	
}

/*从DNS应答报文中解析出IPV4地址，返回值为NULL表示获取失败*/
DnsQueryResult *DomainNameIpv4Addr(struct stream *s)
{
	int i;
	DomainNameHeader header;/*报文头部*/
	DomainNameQuestion *Question = NULL;/*问题部分*/
	DomainNameAnswer **Answer = NULL;/*响应部分*/
	DnsQueryResult *Result = NULL;
	
/*检查是否是合法的DNS回应报文*/
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
	{/*不是DNS回应报文*/
		fprintf(stderr, "not response dns packet.\n");
		goto ERROR;
	}
	
	fprintf(stdout, "domain query nums: %d, domain answer nums: %d\n", header.Questions, header.AnswerRRS);

	/*自己构造的ＤＮＳ请求只有一个问题，所以应答报文也应只有一个问题*/
	if(header.Questions != 1)
	{
		fprintf(stderr, "question nums is not 1.\n");
		goto ERROR;
	}
	
	/*先解析DNS问题，格式:Name + Type + Class*/
	Question = DomainNameQuestionParse(s);
	if(NULL == Question)
		goto ERROR;
	/*解析出DNS问题部分的Name不为空*/
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

/*走到这里，说明DNS响应被期望*/
	if(header.AnswerRRS <= 0)
	{
		fprintf(stderr, "no answer.\n");
		
		goto DONE;
	}
		
	/*再解析DNS相应*/
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


/*若直接跳到DONE，则返回值Result没有响应信息(TimeLive和Address)*/	
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
	/*检查报文来源是否正确*/
	if(from.sin_addr.s_addr != peer->remoteAddr.sin_addr.s_addr || from.sin_port != peer->remoteAddr.sin_port)
	{
		/*不是期望报文*/
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

	if(!dns_result->Address)/*DNS回应报文中没有应答*/
		goto done;
	
	server_peer->remoteAddr.sin_addr.s_addr = htonl(dns_result->Address);
	server_peer->remote_hostip = dns_result->Address;
	//server_peer->remote_hostname = XSTRDUP(MTYPE_HOSTNAME_STRING, ip2str(dns_result->Address));

	/*start to establish tcp link*/
	pop_tcp_client_event(server_peer, pop_tcp_client_event_start);

	/*添加DNS缓存*/
	dns_result_tmp = DnsCacheLookupByHostname(popd->http_proxy_conf.dns_cache, dns_result->HostName);
	if(dns_result_tmp)
	{
		dns_result_tmp->ValidityEndTime = dns_result->ValidityEndTime;
		goto done;/*缓存已存在*/
	}
	
	/*若DNS缓存超出阈值则删除原有的1/3新鲜度较低的DNS缓存*/
	DnsCacheCheckAndDelete(popd->http_proxy_conf.dns_cache);

	/*添加缓存*/
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

	/*非阻塞recvfrom*/
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

	/*数据报接收成功*/
	
	/*根据peer的服务类型进行处理*/
	switch(peer->service_type)
	{
		/*域名解析客户端: 只用于HTTP代理解析域名时使用*/
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

/*UDP读写事件,暂时不用*/
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

/*DNS等待回应队列接口*/
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

/*DNS回应机制*/
void DnsWaitAckTimeoutCall(struct event_node *node)
{
	struct peer *peer = EVENT_ARG(node);
	peer->t_DnsWaitAck = NULL;

	DnsWaitAckQueueDel(popd->http_proxy_conf.dns_ack_wait_queue, peer);
	/*断开pair peer对的TCP连接*/
	pop_tcp_close_paired_peer(peer);
	/**/
	#warning "后续可添加WEB回应：域名解析超时"
}

/*等待回应定时器*/
void DnsWaitAckTimerSet(struct peer *peer)
{
	TIMER_ON(peer->t_DnsWaitAck, DnsWaitAckTimeoutCall, peer, DNS_WAIT_ACK_TIMEOUT);
}

void DnsWaitAckTimerCancel(struct peer* peer)
{
	TIMER_OFF(peer->t_DnsWaitAck);
}

