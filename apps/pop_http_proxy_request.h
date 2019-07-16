#ifndef __POP_HTTP_PROXY_REQUEST_H__
#define __POP_HTTP_PROXY_REQUEST_H__
#include "pop_paired_peer.h"
#include "pop_http_header.h"

#define HTTP_METHOD_CONNECT_ESTABLISHED \
	"HTTP/1.0 200 Connection established\r\n\r\n"

/*HTTP Method*/
typedef struct
{
	const char method[MAX_HTTP_STARTLINE_METHOD_SIZE];
	int (*callback)(struct peer *peer);
}REQUEST_CALLBACK;

/*
ƥ���ַ�����[HTTP METHOD]:
	�ɹ�������[HTTP METHOD]��Ӧ��REQUEST_CALLBACK�ṹ��
	ʧ�ܣ�����NULL
*/
const REQUEST_CALLBACK *pop_http_proxy_method_match(const char *request, unsigned int len);

int pop_http_proxy_request_event(struct peer *peer);


#endif
