#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include "pop_socket.h"
#include "popd.h"
#include "memory.h"
#include "pop_http_header.h"
int sockopt_reuseaddr(int sock)
{
	int ret = -1;
	int on = 1;
	ret = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, 
			(void *)&on, sizeof(on));
	if (ret < 0)
	{
		fprintf(stderr, "can't set sockopt SO_REUSEADDR to socket %d\n", sock);
		return -1;
	}

	return 0;
}

#ifdef SO_REUSEPORT
int sockopt_reuseport (int sock)
{
	int ret;
	int on = 1;

	ret = setsockopt (sock, SOL_SOCKET, SO_REUSEPORT, 
			(void *) &on, sizeof (on));
	if (ret < 0)
	{
		fprintf(stderr, "can't set sockopt SO_REUSEPORT to socket %d\n", sock);
		return -1;
	}
	
	return 0;
}
#else
int sockopt_reuseport (int sock)
{
	return 0;
}
#endif /* 0 */

int pop_tcp_socket(struct peer *peer)
{
#ifdef DNSPARSE_BLOCK
	int sockfd = -1;
	struct addrinfo *addr = peer->addr_ptr;
	if(NULL == addr)
		goto ERROR;
	
	sockfd = socket(addr->ai_family/*AF_INET*/, addr->ai_socktype/*SOCK_STREAM*/, addr->ai_protocol);
	if(sockfd < 0)
	{
		fprintf(stderr, "socket error: %s\n", safe_strerror(errno));
		goto ERROR;
	}

	return sockfd;
	
ERROR:
	return -1;
#else
	int sockfd = -1;
	
	sockfd = socket(peer->remoteAddr.sin_family/*AF_INET*/, SOCK_STREAM, 0);
	if(sockfd < 0)
	{
		fprintf(stderr, "socket error: %s\n", safe_strerror(errno));
		goto ERROR;
	}

	return sockfd;
	
ERROR:
	return -1;

#endif
}

int pop_tcp_connect_unblock(struct peer *peer)
{
	int val, ret;
	val = fcntl (peer->sockfd, F_GETFL, 0);
	fcntl(peer->sockfd, F_SETFL, val|O_NONBLOCK);

	ret = connect(peer->sockfd, (struct sockaddr *)&peer->remoteAddr, sizeof(peer->remoteAddr));
	if(ret == 0)/* Immediate success */
	{
		fcntl (peer->sockfd, F_SETFL, val);
		return CONNECT_SUCCESS;
	}

	/* If connect is in progress then return 1 else it's real error. */
	if(ret < 0)
	{
		if(errno != EINPROGRESS)
		{
			fprintf(stderr, "can't connect to %s fd %d : %s",
				peer->remote_hostname, peer->sockfd, safe_strerror(errno));
			fcntl(peer->sockfd, F_SETFL, val);
			return CONNECT_ERROR;
		}
	}

	fcntl(peer->sockfd, F_SETFL, val);
	return CONNECT_IN_PROGRESS;
}

/*通用接口：非阻塞accept，返回值：accept套接字*/
int pop_tcp_accept_unblock(int sockfd, struct sockaddr *src_addr)
{
	int val, client_sock;
	socklen_t len = sizeof(struct sockaddr_in);
	val = fcntl(sockfd, F_GETFL, 0);
	fcntl(sockfd, F_SETFL, val|O_NONBLOCK);
	client_sock = accept(sockfd, src_addr, &len);
	fcntl(sockfd, F_SETFL, val);

	return client_sock;
}

/*域名转换*/
struct addrinfo *DomainTrans_Gethostbyname(char *data)
{
	int ret;
	char *Host = NULL, *Port = NULL, *ptr = NULL;
	struct addrinfo hints, *res;
	if(NULL == data)
		return NULL;

	Host = XSTRDUP(MTYPE_STRING, data);
	/*check whether port existed*/
	ptr = strstr(Host, ":");
	if(NULL != ptr)
	{
		*ptr = '\0';/*Split*/
		Port = ptr+1;
		fprintf(stdout, "H:%s, P:%s\n", Host ,Port);
	}

	bzero(&hints, sizeof(hints));
	hints.ai_flags = AI_CANONNAME;
	hints.ai_family = AF_INET;			/*IPV4*/
	hints.ai_socktype = SOCK_STREAM;	/*流套接字*/

	if(Port)
		ret = getaddrinfo(Host, Port, &hints, &res);
	else
		ret = getaddrinfo(Host, DEFAULT_HTTP_PORT_STR/*"80"*/, &hints, &res);

	if(ret != 0)
	{
		fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(ret));
		goto ERROR;
	}

	XFREE(MTYPE_STRING, Host);
	return res;

ERROR:
	if(Host)
		XFREE(MTYPE_STRING, Host);

	return NULL;
}

#define MAX_ADDR_BUFFER		10
static int current_buffer = 0;
static char addr_buffer[MAX_ADDR_BUFFER][INET_ADDRSTRLEN];

const char* ip2str(u_int32_t addr)
{
    char *buffer = addr_buffer[current_buffer];
    ZEROMEMORY(buffer, INET_ADDRSTRLEN);
    addr = htonl (addr);
    inet_ntop (AF_INET, (char *) &addr, buffer, INET_ADDRSTRLEN);
    current_buffer++;
    if (current_buffer >= MAX_ADDR_BUFFER)
        current_buffer = 0;

    return buffer;
}

