#ifndef __POP_SOCKET_H__
#define __POP_SOCKET_H__

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include "pop_paired_peer.h"
#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif /* INET_ADDRSTRLEN */

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif /* INET6_ADDRSTRLEN */

#ifndef INET6_BUFSIZ
#define INET6_BUFSIZ 51
#endif /* INET6_BUFSIZ */

enum connect_result
{
	CONNECT_ERROR,
	CONNECT_SUCCESS,
	CONNECT_IN_PROGRESS
};

int sockopt_reuseaddr(int sock);
int sockopt_reuseport (int sock);
/*通用接口：非阻塞accept，返回值：accept套接字*/
int pop_tcp_accept_unblock(int sockfd, struct sockaddr *src_addr);
/*域名转换*/
struct addrinfo *DomainTrans_Gethostbyname(char *hostname);

#define FREEADDRINFO(X) \
	do{ \
		freeaddrinfo(X); \
		X = NULL; \
	}while(0)

#define GETHOSTBYNAME(X, H) \
	do{ \
		if((X)->addr_res) \
		{ \
			FREEADDRINFO((X)->addr_res); \
			(X)->addr_ptr = NULL; \
		} \
		(X)->addr_res = DomainTrans_Gethostbyname(H); \
		(X)->addr_ptr = (X)->addr_res; \
	}while(0)

#define ADDRINFO_NEXT(A) (((struct addrinfo *)(A))->ai_next)

#define ALL_ADDRINFO_NODE_RO(addr,data) \
	(data) = addr; \
	(data) != NULL; \
	(data) = ADDRINFO_NEXT(addr)


const char* ip2str(u_int32_t addr);
int pop_tcp_connect_unblock(struct peer *peer);
int pop_tcp_socket(struct peer *peer);


#endif
