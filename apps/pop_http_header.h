#ifndef __POP_HTTP_HEADER_H__
#define __POP_HTTP_HEADER_H__
#include "pop_paired_peer.h"
#define DEFAULT_HTTP_PORT			80
#define DEFAULT_HTTP_PORT_STR		"80"
#define DEFAULT_HTTPS_PORT			443
#define DEFAULT_HTTPS_PORT_STR		"443"

/*起始行+首部+主体*/
/*起始行*/
#define MAX_HTTP_STARTLINE_METHOD_SIZE		15
#define MAX_HTTP_STARTLINE_URL_SIZE			200

#define HTTP_METHOD_GET			"GET"
#define HTTP_METHOD_HEAD		"HEAD"
#define HTTP_METHOD_PUT			"PUT"
#define HTTP_METHOD_POST		"POST"
#define HTTP_METHOD_TRACE		"TRACE"
#define HTTP_METHOD_OPTIONS		"OPTIONS"
#define HTTP_METHOD_DELETE		"DELETE"
#define HTTP_METHOD_LOCK		"LOCK"
#define HTTP_METHOD_MKCOL		"MKCOL"
#define HTTP_METHOD_COPY		"COPY"
#define HTTP_METHOD_MOVE		"MOVE"
#define HTTP_METHOD_CONNECT		"CONNECT"	/*HTTP隧道*/
/*MAX METHOD COUNTS*/
#define HTTP_METHOD_MAX			12

#define HTTP_HEADER_HOST		"Host"

#define HTTP_HEADER_SUFFIX		"\r\n"
#define HTTP_HEADER_ENDLINE		"\r\n\r\n"

/*请求起始行*/
#define HTTP_METHOD_LEN		32
#define HTTP_VERSION_LEN	32
typedef struct http_request_sline
{
	char Method[HTTP_METHOD_LEN];		/*HTTP方法*/
	char *URI;			/*URL*/
	char Http_v[HTTP_VERSION_LEN];	/*HTTP VERSION*/				
}HTTP_REQUEST_SLINE;

/*回应起始行*/
#define HTTP_CODE_DES_LEN	128
typedef struct http_response_sline
{
	char http_v[HTTP_VERSION_LEN];
	unsigned int http_code;
	char code_des[HTTP_CODE_DES_LEN];
}HTTP_RESPONSE_SLINE;

/*通用首部*/
/*请求首部*/
/*回应首部*/
/*实体首部*/
struct http_header
{
	unsigned char field;
	unsigned char value[0];
};

struct http_headers
{
	int numb;
	struct http_header *data;
};

int pop_get_remoteIp_from_addrinfo(struct peer *peer);
char *HttpHeaderParse(char *data, char *headerKey);
void GetHostIpPostFromHost(char *url, char *ipAddr, int *port);
void HttpRequestSlineFree(void *l);
HTTP_REQUEST_SLINE *HttpRequestStartLineParse(const char *data);
HTTP_RESPONSE_SLINE *HttpResponseStartLineParse(const char *data);

int GetHostIpPostFromURI(char *URI, char *ipAddr, int *port);

#endif
