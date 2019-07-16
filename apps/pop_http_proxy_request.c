#include "pop_http_proxy_request.h"
#include "pop_paired_peer.h"
#include "memory.h"
#include "pop_socket.h"
#include "popd.h"
#include "pop_tcp_fsm.h"
#include "pop_network.h"
#include "pop_transpond.h"
#include <arpa/inet.h>
#include "pop_http_header.h"
#include "pop_domain_cache.h"
#include "pop_httpcache.h"

int pop_get_remoteIp_from_addrinfo(struct peer *peer)
{
	struct sockaddr_in *sockaddr = NULL;
	if(NULL == peer->addr_ptr)
		return -1;

	sockaddr = ((struct sockaddr_in *)(peer->addr_ptr->ai_addr));
	/*Get remote Ip and Port*/
	peer->remote_hostip = ntohl(sockaddr->sin_addr.s_addr);
	if(peer->remote_hostname)
		XFREE(MTYPE_HOSTNAME_STRING, peer->remote_hostname);
	peer->remote_hostname = XSTRDUP(MTYPE_HOSTNAME_STRING, ip2str(peer->remote_hostip));
	peer->remote_port = ntohs(sockaddr->sin_port);
	peer->remoteAddr = *sockaddr;

	/*point the next addr info*/
	/*这里用于进行多次连接*/
	//peer->addr_ptr = ADDRINFO_NEXT(peer->addr_ptr);
	
	fprintf(stdout, "Get remoteIp: %s, remotePort:%d\n", peer->remote_hostname, peer->remote_port);
	return 0;	
}

#ifdef DNSPARSE_BLOCK //阻塞式DNS域名查询
//#if 1
/*本机为服务端*/
int pop_http_proxy_request_get(struct peer *client_peer)
{
	int ret = -1;
	char *HttpHost = NULL;
	struct stream *s = NULL;
	struct peer *server_peer = NULL;
	//fprintf(stdout, "read buffer from %s:\n%s\n", client_peer->remote_hostname,client_peer->recv_buffer->data);
	/*Parse Packet:get Host header*/
	HttpHost = HttpHeaderParse((char *)STREAM_DATA(client_peer->recv_buffer), HTTP_HEADER_HOST);
	if(NULL == HttpHost)
		goto ERROR;

	server_peer = pop_peer_new();
	server_peer->service_type = pop_service_type_http_proxy;	/*HTTP PROXY*/
	server_peer->tcp_peer_type = pop_tcp_peer_type_server;		/*peer is server*/
	server_peer->tcp_client_status = Pop_Tcp_Client_Idle;	
	
	server_peer->paired_peer = client_peer;
	client_peer->paired_peer = server_peer;

	/*DomainParse Host*/
	GETHOSTBYNAME(server_peer, HttpHost);
	if(NULL == server_peer->addr_res)
		goto ERROR;

	/*get one of the result of addrinfo*/
	ret = pop_get_remoteIp_from_addrinfo(server_peer);
	if(ret < 0)
		goto ERROR;

	s = stream_dup(client_peer->recv_buffer);
	stream_fifo_push(server_peer->write_fifo, s);
	/*start to establish tcp link*/
	pop_tcp_client_event(server_peer, pop_tcp_client_event_start);
	
	XFREE(MTYPE_STRING, HttpHost);
	return 0;
ERROR:
	fprintf(stderr, "<%s, %d>parse error!!\n", __FUNCTION__, __LINE__);
	if(HttpHost)
		XFREE(MTYPE_STRING, HttpHost);
	/*断开pair peer对的TCP连接*/
	pop_tcp_close_paired_peer(client_peer);


	return -1;
}

int pop_http_proxy_request_head(struct peer *client_peer)
{
	int ret = -1;
	char *HttpHost = NULL;
	struct stream *s = NULL;
	struct peer *server_peer = NULL;
	//fprintf(stdout, "read buffer from %s:\n%s\n", client_peer->remote_hostname,client_peer->recv_buffer->data);
	/*Parse Packet:get Host header*/
	HttpHost = HttpHeaderParse((char *)STREAM_DATA(client_peer->recv_buffer), HTTP_HEADER_HOST);
	if(NULL == HttpHost)
		goto ERROR;

	server_peer = pop_peer_new();
	server_peer->service_type = pop_service_type_http_proxy;	/*HTTP PROXY*/
	server_peer->tcp_peer_type = pop_tcp_peer_type_server;		/*peer is server*/
	server_peer->tcp_client_status = Pop_Tcp_Client_Idle;	
	
	server_peer->paired_peer = client_peer;
	client_peer->paired_peer = server_peer;

	/*DomainParse Host*/
	GETHOSTBYNAME(server_peer, HttpHost);
	if(NULL == server_peer->addr_res)
		goto ERROR;

	/*get one of the result of addrinfo*/
	ret = pop_get_remoteIp_from_addrinfo(server_peer);
	if(ret < 0)
		goto ERROR;

	s = stream_dup(client_peer->recv_buffer);
	stream_fifo_push(server_peer->write_fifo, s);
	/*start to establish tcp link*/
	pop_tcp_client_event(server_peer, pop_tcp_client_event_start);
	
	XFREE(MTYPE_STRING, HttpHost);
	return 0;
ERROR:
	fprintf(stderr, "<%s, %d>parse error!!\n", __FUNCTION__, __LINE__);
	if(HttpHost)
		XFREE(MTYPE_STRING, HttpHost);
	/*断开pair peer对的TCP连接*/
	pop_tcp_close_paired_peer(client_peer);

	return -1;
}

int pop_http_proxy_request_put(struct peer *client_peer)
{
	int ret = -1;
	char *HttpHost = NULL;
	struct stream *s = NULL;
	struct peer *server_peer = NULL;
	//fprintf(stdout, "read buffer from %s:\n%s\n", client_peer->remote_hostname,client_peer->recv_buffer->data);
	/*Parse Packet:get Host header*/
	HttpHost = HttpHeaderParse((char *)STREAM_DATA(client_peer->recv_buffer), HTTP_HEADER_HOST);
	if(NULL == HttpHost)
		goto ERROR;

	server_peer = pop_peer_new();
	server_peer->service_type = pop_service_type_http_proxy;	/*HTTP PROXY*/
	server_peer->tcp_peer_type = pop_tcp_peer_type_server;		/*peer is server*/
	server_peer->tcp_client_status = Pop_Tcp_Client_Idle;	
	
	server_peer->paired_peer = client_peer;
	client_peer->paired_peer = server_peer;

	/*DomainParse Host*/
	GETHOSTBYNAME(server_peer, HttpHost);
	if(NULL == server_peer->addr_res)
		goto ERROR;

	/*get one of the result of addrinfo*/
	ret = pop_get_remoteIp_from_addrinfo(server_peer);
	if(ret < 0)
		goto ERROR;

	s = stream_dup(client_peer->recv_buffer);
	stream_fifo_push(server_peer->write_fifo, s);
	/*start to establish tcp link*/
	pop_tcp_client_event(server_peer, pop_tcp_client_event_start);
	
	XFREE(MTYPE_STRING, HttpHost);
	return 0;
ERROR:
	fprintf(stderr, "<%s, %d>parse error!!\n", __FUNCTION__, __LINE__);
	if(HttpHost)
		XFREE(MTYPE_STRING, HttpHost);
	/*断开pair peer对的TCP连接*/
	pop_tcp_close_paired_peer(client_peer);


	return -1;
}
int pop_http_proxy_request_post(struct peer *client_peer)
{
	int ret = -1;
	char *HttpHost = NULL;
	struct stream *s = NULL;
	struct peer *server_peer = NULL;
	//fprintf(stdout, "read buffer from %s:\n%s\n", client_peer->remote_hostname,client_peer->recv_buffer->data);
	/*Parse Packet:get Host header*/
	HttpHost = HttpHeaderParse((char *)STREAM_DATA(client_peer->recv_buffer), HTTP_HEADER_HOST);
	if(NULL == HttpHost)
		goto ERROR;

	server_peer = pop_peer_new();
	server_peer->service_type = pop_service_type_http_proxy;	/*HTTP PROXY*/
	server_peer->tcp_peer_type = pop_tcp_peer_type_server;		/*peer is server*/
	server_peer->tcp_client_status = Pop_Tcp_Client_Idle;	
	
	server_peer->paired_peer = client_peer;
	client_peer->paired_peer = server_peer;

	/*DomainParse Host*/
	GETHOSTBYNAME(server_peer, HttpHost);
	if(NULL == server_peer->addr_res)
		goto ERROR;

	/*get one of the result of addrinfo*/
	ret = pop_get_remoteIp_from_addrinfo(server_peer);
	if(ret < 0)
		goto ERROR;

	s = stream_dup(client_peer->recv_buffer);
	stream_fifo_push(server_peer->write_fifo, s);
	/*start to establish tcp link*/
	pop_tcp_client_event(server_peer, pop_tcp_client_event_start);
	
	XFREE(MTYPE_STRING, HttpHost);
	return 0;
ERROR:
	fprintf(stderr, "<%s, %d>parse error!!\n", __FUNCTION__, __LINE__);
	if(HttpHost)
		XFREE(MTYPE_STRING, HttpHost);
	/*断开pair peer对的TCP连接*/
	pop_tcp_close_paired_peer(client_peer);


	return -1;
}

int pop_http_proxy_request_trace(struct peer *client_peer)
{
	fprintf(stdout, "read buffer from %s:\n%s\n", client_peer->remote_hostname,client_peer->recv_buffer->data);
	return 0;
}

int pop_http_proxy_request_options(struct peer *client_peer)
{
	fprintf(stdout, "read buffer from %s:\n%s\n", client_peer->remote_hostname,client_peer->recv_buffer->data);
	return 0;
}

int pop_http_proxy_request_delete(struct peer *client_peer)
{
	fprintf(stdout, "read buffer from %s:\n%s\n", client_peer->remote_hostname,client_peer->recv_buffer->data);
	return 0;
}

int pop_http_proxy_request_lock(struct peer *client_peer)
{
	fprintf(stdout, "read buffer from %s:\n%s\n", client_peer->remote_hostname,client_peer->recv_buffer->data);
	return 0;
}

int pop_http_proxy_request_mkcol(struct peer *client_peer)
{
	fprintf(stdout, "read buffer from %s:\n%s\n", client_peer->remote_hostname,client_peer->recv_buffer->data);
	return 0;
}

int pop_http_proxy_request_copy(struct peer *client_peer)
{
	fprintf(stdout, "read buffer from %s:\n%s\n", client_peer->remote_hostname,client_peer->recv_buffer->data);
	return 0;
}

int pop_http_proxy_request_move(struct peer *client_peer)
{
	fprintf(stdout, "read buffer from %s:\n%s\n", client_peer->remote_hostname,client_peer->recv_buffer->data);
	return 0;
}

int pop_http_proxy_request_connect(struct peer *client_peer)
{
	int ret = -1;
	char *HttpHost = NULL;
	struct stream *s = NULL;
	struct peer *server_peer = NULL;
	size_t writelen;
	//fprintf(stdout, "read buffer from %s:\n%s\n", client_peer->remote_hostname,client_peer->recv_buffer->data);
	/*Parse Packet:get Host header*/
	//HttpHost = HttpHeaderParseHost((char *)STREAM_DATA(client_peer->recv_buffer));
	HttpHost = HttpHeaderParse((char *)STREAM_DATA(client_peer->recv_buffer), HTTP_HEADER_HOST);
	if(NULL == HttpHost)
		goto ERROR;

	server_peer = pop_peer_new();
	server_peer->service_type = pop_service_type_http_proxy;	/*HTTP PROXY*/
	server_peer->tcp_peer_type = pop_tcp_peer_type_server;		/*peer is server*/
	server_peer->tcp_client_status = Pop_Tcp_Client_Idle;	
	
	server_peer->paired_peer = client_peer;
	client_peer->paired_peer = server_peer;

	/*DomainParse Host*/
	GETHOSTBYNAME(server_peer, HttpHost);
	if(NULL == server_peer->addr_res)
		goto ERROR;

	/*get one of the result of addrinfo*/
	ret = pop_get_remoteIp_from_addrinfo(server_peer);
	if(ret < 0)
		goto ERROR;

	/*Connect 方法：向客户端发送CONNECT ESTABLISHED报文*/
	writelen = strlen(HTTP_METHOD_CONNECT_ESTABLISHED);
	s = stream_new(writelen);
	stream_put(s, HTTP_METHOD_CONNECT_ESTABLISHED, writelen);
	//pop_tcp_packet_to_write_fifo(client_peer, s);
	stream_fifo_push(client_peer->write_fifo, s);//将CONNECT回应加入队列等待对端peer连接成功再发送
	server_peer->if_tunnel = POP_HTTP_CONNECT_TUNNEL;

	/*start to establish tcp link*/
	pop_tcp_client_event(server_peer, pop_tcp_client_event_start);
	
	XFREE(MTYPE_STRING, HttpHost);
	return 0;
ERROR:
	fprintf(stderr, "<%s, %d>parse error!!\n", __FUNCTION__, __LINE__);
	if(HttpHost)
		XFREE(MTYPE_STRING, HttpHost);

	/*断开pair peer对的TCP连接*/
	pop_tcp_close_paired_peer(client_peer);

	return -1;
}

const REQUEST_CALLBACK POP_REQUEST_METHOD_CALLBACK[HTTP_METHOD_MAX] = 
{
	{HTTP_METHOD_GET, 		pop_http_proxy_request_get},	/*RECEIVE HTTP GET*/
	{HTTP_METHOD_HEAD,		pop_http_proxy_request_head},
	{HTTP_METHOD_PUT,		pop_http_proxy_request_put},
	{HTTP_METHOD_POST,		pop_http_proxy_request_post},
	{HTTP_METHOD_TRACE,		pop_http_proxy_request_trace},
	{HTTP_METHOD_OPTIONS,	pop_http_proxy_request_options},
	{HTTP_METHOD_DELETE,	pop_http_proxy_request_delete},
	{HTTP_METHOD_LOCK,		pop_http_proxy_request_lock},
	{HTTP_METHOD_MKCOL,		pop_http_proxy_request_mkcol},
	{HTTP_METHOD_COPY,		pop_http_proxy_request_copy},
	{HTTP_METHOD_MOVE,		pop_http_proxy_request_move},
	{HTTP_METHOD_CONNECT,	pop_http_proxy_request_connect}
};

/*
匹配字符串与[HTTP METHOD]:
	成功：返回[HTTP METHOD]对应的REQUEST_CALLBACK结构；
	失败：返回NULL
*/
const REQUEST_CALLBACK *pop_http_proxy_method_match(const char *request, unsigned int len)
{
	int i;
	for(i = 0; i < HTTP_METHOD_MAX; i++)
	{
		if(len < MAX_HTTP_STARTLINE_METHOD_SIZE)
			continue;
		
		if(0 == memcmp(request, POP_REQUEST_METHOD_CALLBACK[i].method, strlen(POP_REQUEST_METHOD_CALLBACK[i].method)))
			return &POP_REQUEST_METHOD_CALLBACK[i];
	}
	return NULL;
}

#else
static struct peer* pop_remote_server_peer_make(const char *ipAddr, int port)
{
	struct peer *server_peer = NULL;
	server_peer = pop_peer_new();
	server_peer->service_type = pop_service_type_http_proxy;	/*HTTP PROXY*/
	server_peer->tcp_peer_type = pop_tcp_peer_type_server;		/*peer is server*/
	server_peer->tcp_client_status = Pop_Tcp_Client_Idle;	

	inet_aton(ipAddr, &server_peer->remoteAddr.sin_addr);/*若转换失败则说明不是IP地址，需要进行DNS解析*/
	if(server_peer->remoteAddr.sin_addr.s_addr)
		server_peer->remote_hostip = ntohl(server_peer->remoteAddr.sin_addr.s_addr);

	server_peer->remoteAddr.sin_port = htons(port);
	server_peer->remoteAddr.sin_family = AF_INET;

	server_peer->remote_hostname = XSTRDUP(MTYPE_HOSTNAME_STRING, ipAddr);
	server_peer->remote_port = port;
	fprintf(stdout, "Make server peer, remoteIp: %s, remotePort:%d\n", server_peer->remote_hostname, server_peer->remote_port);

	return server_peer;
}

static int pop_get_host_by_name_unblock(struct peer *peer)
{
	int ret;
	ret = get_host_by_name_unblock(peer->remote_hostname);
	if(ret < 0)
	{/*查询失败*/
		return -1;
	}

	/*记录DNS标识，将peer加入等待队列*/
	peer->DnsTransactionID = DomainTransID - 1;
	DnsWaitAckQueueAdd(popd->http_proxy_conf.dns_ack_wait_queue, peer);	
	
	/*发起查询成功，等待回应*/
	DnsWaitAckTimerSet(peer);

	return 0;
}

/*本机为服务端*/
int pop_http_proxy_request_get(struct peer *client_peer)
{
	struct stream *s = NULL;
	struct peer *server_peer = NULL;
	char ipAddr[255] = {0};
	int port, ret;
	HTTP_REQUEST_SLINE *SLINE = NULL;
	DnsQueryResult *DnsResult = NULL;
	
	/*解析报文，获取起始行*/
	SLINE = HttpRequestStartLineParse((const char *)STREAM_DATA(client_peer->recv_buffer));
	if(NULL == SLINE)
		goto ERROR;

	/*获取主机IP和PORT*/
	ret = GetHostIpPostFromURI(SLINE->URI, ipAddr, &port);
	if(ret < 0)
	{
		goto ERROR;
	}
	server_peer = pop_remote_server_peer_make(ipAddr, port);
	if(NULL == server_peer)
	{
		goto ERROR;
	}
	pop_peers_paired(server_peer, client_peer);	
	/*将要发送的报文放入服务对端以连接建立后发送报文*/
	s = stream_dup(client_peer->recv_buffer);
	stream_fifo_push(server_peer->write_fifo, s);	

	/*DomainParse Host*/
	if(server_peer->remote_hostip)
	{/*是IPV4地址，直接连接对端服务器*/		
		/*start to establish tcp link*/
		pop_tcp_client_event(server_peer, pop_tcp_client_event_start);
	}
	else
	{/*不是IPV4地址，先解析域名*/
		/*先查询DNS缓存*/
		fprintf(stdout, "Look up Dns cache: %s\n", server_peer->remote_hostname);

		unsigned int currentTime = time(NULL);

		DnsResult = DnsCacheLookupByHostname(popd->http_proxy_conf.dns_cache, server_peer->remote_hostname);
		if(DnsResult && currentTime <= DnsResult->ValidityEndTime)
		{/*DNS缓存命中且未过期*/
			server_peer->remote_hostip = DnsResult->Address;
			server_peer->remoteAddr.sin_addr.s_addr = htonl(server_peer->remote_hostip);

			/*命中后更新缓存新鲜度*/
			DnsCacheFreshnessUpdate(popd->http_proxy_conf.dns_cache, DnsResult);
			
			fprintf(stdout, "find dns cache : %s %s\n", server_peer->remote_hostname, ip2str(server_peer->remote_hostip));
			/*start to establish tcp link*/
			pop_tcp_client_event(server_peer, pop_tcp_client_event_start);
		}
		else
		{/*DNS缓存未命中或已过期，查询DNS*/
			if(DnsResult)//DNS缓存过期
				DnsCacheDel(popd->http_proxy_conf.dns_cache, DnsResult);
		
			if(pop_get_host_by_name_unblock(server_peer) < 0)
			{
				goto ERROR;
			}
		}
	}
	
	HttpRequestSlineFree(SLINE);
	return 0;
ERROR:
	if(SLINE)
		HttpRequestSlineFree(SLINE);

	fprintf(stderr, "<%s, %d>get error!!\n", __FUNCTION__, __LINE__);
	
	/*断开pair peer对的TCP连接*/
	pop_tcp_close_paired_peer(client_peer);

	return -1;
}

int pop_http_proxy_request_head(struct peer *client_peer)
{
	return pop_http_proxy_request_get(client_peer);
}

int pop_http_proxy_request_put(struct peer *client_peer)
{
	return pop_http_proxy_request_get(client_peer);
}

int pop_http_proxy_request_post(struct peer *client_peer)
{
	return pop_http_proxy_request_get(client_peer);
}


int pop_http_proxy_request_trace(struct peer *client_peer)
{
	fprintf(stdout, "read buffer from %s:\n%s\n", client_peer->remote_hostname,client_peer->recv_buffer->data);
	return 0;
}

int pop_http_proxy_request_options(struct peer *client_peer)
{
	fprintf(stdout, "read buffer from %s:\n%s\n", client_peer->remote_hostname,client_peer->recv_buffer->data);
	return 0;
}

int pop_http_proxy_request_delete(struct peer *client_peer)
{
	fprintf(stdout, "read buffer from %s:\n%s\n", client_peer->remote_hostname,client_peer->recv_buffer->data);
	return 0;
}

int pop_http_proxy_request_lock(struct peer *client_peer)
{
	fprintf(stdout, "read buffer from %s:\n%s\n", client_peer->remote_hostname,client_peer->recv_buffer->data);
	return 0;
}

int pop_http_proxy_request_mkcol(struct peer *client_peer)
{
	fprintf(stdout, "read buffer from %s:\n%s\n", client_peer->remote_hostname,client_peer->recv_buffer->data);
	return 0;
}

int pop_http_proxy_request_copy(struct peer *client_peer)
{
	fprintf(stdout, "read buffer from %s:\n%s\n", client_peer->remote_hostname,client_peer->recv_buffer->data);
	return 0;
}

int pop_http_proxy_request_move(struct peer *client_peer)
{
	fprintf(stdout, "read buffer from %s:\n%s\n", client_peer->remote_hostname,client_peer->recv_buffer->data);
	return 0;
}


int pop_http_proxy_request_connect(struct peer *client_peer)
{
	struct stream *s = NULL;
	struct peer *server_peer = NULL;
	char ipAddr[255] = {0};
	int port, ret;
	HTTP_REQUEST_SLINE *SLINE = NULL;
	DnsQueryResult *DnsResult = NULL;

	size_t writelen;
	/*解析报文，获取起始行*/
	SLINE = HttpRequestStartLineParse((const char *)STREAM_DATA(client_peer->recv_buffer));
	if(NULL == SLINE)
		goto ERROR;	

	/*获取主机IP和PORT*/
	ret = GetHostIpPostFromURI(SLINE->URI, ipAddr, &port);
	if(ret < 0)
		goto ERROR;
	//port = DEFAULT_HTTPS_PORT;
	server_peer = pop_remote_server_peer_make(ipAddr, port);
	if(NULL == server_peer)
		goto ERROR;
	
	pop_peers_paired(server_peer, client_peer); 
	
	/*Connect 方法：与服务端的TCP连接建立后，要向客户端发送"CONNECT ESTABLISHED"报文*/
	writelen = strlen(HTTP_METHOD_CONNECT_ESTABLISHED);
	s = stream_new(writelen);
	stream_put(s, HTTP_METHOD_CONNECT_ESTABLISHED, writelen);
	stream_fifo_push(client_peer->write_fifo, s);//将CONNECT回应加入队列等待对端peer连接成功再发送
	server_peer->if_tunnel = POP_HTTP_CONNECT_TUNNEL;

	/*DomainParse Host*/
	if(server_peer->remote_hostip)
	{/*是IPV4地址，直接连接对端服务器*/
		/*start to establish tcp link*/
		pop_tcp_client_event(server_peer, pop_tcp_client_event_start);
	}
	else
	{/*不是IPV4地址，先解析域名*/	
		/*先查询DNS缓存*/
		fprintf(stdout, "Look up Dns cache: %s\n", server_peer->remote_hostname);

		unsigned int currentTime = time(NULL);
		
		DnsResult = DnsCacheLookupByHostname(popd->http_proxy_conf.dns_cache, server_peer->remote_hostname);
		if(DnsResult && currentTime <= DnsResult->ValidityEndTime)
		{/*DNS缓存命中且未过期*/
			server_peer->remote_hostip = DnsResult->Address;
			server_peer->remoteAddr.sin_addr.s_addr = htonl(server_peer->remote_hostip);

			/*命中后更新缓存新鲜度*/
			DnsCacheFreshnessUpdate(popd->http_proxy_conf.dns_cache, DnsResult);
			
			fprintf(stdout, "find dns cache : %s %s\n", server_peer->remote_hostname, ip2str(server_peer->remote_hostip));
			/*start to establish tcp link*/
			pop_tcp_client_event(server_peer, pop_tcp_client_event_start);
		}
		else
		{/*DNS缓存未命中，查询DNS*/
			if(pop_get_host_by_name_unblock(server_peer) < 0)
			{
				goto ERROR;
			}
		}
	}

	HttpRequestSlineFree(SLINE);
	return 0;
ERROR:
	if(SLINE)
		HttpRequestSlineFree(SLINE);
	fprintf(stderr, "<%s, %d>get error!!\n", __FUNCTION__, __LINE__);
	
	/*断开pair peer对的TCP连接*/
	pop_tcp_close_paired_peer(client_peer);

	return -1;
}


const REQUEST_CALLBACK POP_REQUEST_METHOD_CALLBACK[HTTP_METHOD_MAX] = 
{
	{HTTP_METHOD_GET, 		pop_http_proxy_request_get},	/*RECEIVE HTTP GET*/
	{HTTP_METHOD_HEAD,		pop_http_proxy_request_head},
	{HTTP_METHOD_PUT,		pop_http_proxy_request_put},
	{HTTP_METHOD_POST,		pop_http_proxy_request_post},
	{HTTP_METHOD_TRACE,		pop_http_proxy_request_trace},
	{HTTP_METHOD_OPTIONS,	pop_http_proxy_request_options},
	{HTTP_METHOD_DELETE,	pop_http_proxy_request_delete},
	{HTTP_METHOD_LOCK,		pop_http_proxy_request_lock},
	{HTTP_METHOD_MKCOL,		pop_http_proxy_request_mkcol},
	{HTTP_METHOD_COPY,		pop_http_proxy_request_copy},
	{HTTP_METHOD_MOVE,		pop_http_proxy_request_move},
	{HTTP_METHOD_CONNECT,	pop_http_proxy_request_connect}
};

/*
匹配字符串与[HTTP METHOD]:
	成功：返回[HTTP METHOD]对应的REQUEST_CALLBACK结构；
	失败：返回NULL
*/
const REQUEST_CALLBACK *pop_http_proxy_method_match(const char *request, unsigned int len)
{
	int i;
	for(i = 0; i < HTTP_METHOD_MAX; i++)
	{
		if(len < MAX_HTTP_STARTLINE_METHOD_SIZE)
			continue;
		
		if(0 == memcmp(request, POP_REQUEST_METHOD_CALLBACK[i].method, strlen(POP_REQUEST_METHOD_CALLBACK[i].method)))
			return &POP_REQUEST_METHOD_CALLBACK[i];
	}
	return NULL;
}

#endif



int pop_http_proxy_request_event(struct peer *peer)
{
	int ret = -1;
	struct stream *s = NULL;

	if((peer->paired_peer)
		&& (peer->paired_peer->tcp_client_status != Pop_Tcp_Client_Idle))
	{
		goto DELAY_FORWARD;//正在与服务器对端建立TCP连接，缓存报文，延迟发送
	}
		
	const REQUEST_CALLBACK *REQ_CALLBACK = pop_http_proxy_method_match((const char *)STREAM_DATA(peer->recv_buffer), (unsigned int)stream_get_endp(peer->recv_buffer));
	if(NULL == REQ_CALLBACK)
	{
		/*未知方法的报文则直接转发*/
		fprintf(stdout, "Unkonwn method, directly farwarding.\n");
		goto DELAY_FORWARD;
		return -1;
	}	

	fprintf(stdout, "[REQUEST] Method: %s\n", REQ_CALLBACK->method);
	ret = REQ_CALLBACK->callback(peer);
	if(ret < 0)
		return ret;

	return 0;
DELAY_FORWARD:
	s = stream_dup(peer->recv_buffer);
	stream_fifo_push(peer->write_fifo, s);
	return 0;
}



