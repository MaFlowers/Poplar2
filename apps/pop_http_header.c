#include "util.h"
#include "pop_http_header.h"
#include "memory.h"
#include "pop_socket.h"
#include "pop_comm_hdr.h"
/*
	解析HTTP报文首部字段
	@data: HTTP首部报文
	@headerKey: 要解析的字段
*/
char *HttpHeaderParse(char *data, char *headerKey)
{
	assert(NULL != data && NULL != headerKey);
	
	char *headerValue = NULL;
	char *host_ptr = NULL, *suffix_ptr = NULL;
	int headerValueLen;
	/*find the header Host*/
	host_ptr = strstr(data, headerKey);
	if(NULL == host_ptr)
		return NULL;
	
	/*Get Value's len*/
	host_ptr += strlen(headerKey) + 2;/*多偏移一个冒号和空格*/
	suffix_ptr = strstr(host_ptr, HTTP_HEADER_SUFFIX);/*找到后缀\r\n*/
	if(NULL == suffix_ptr)
		return NULL;
	headerValueLen = suffix_ptr - host_ptr;/*get host len*/
	
	/*Get value "Host"*/
	headerValue = XCALLOC(MTYPE_STRING, headerValueLen + 1);
	memcpy(headerValue, host_ptr, headerValueLen);

	fprintf(stdout, "[%s]:%s\n", headerKey, headerValue);
	return headerValue;
}

void HttpRequestSlineFree(void *l)
{
	HTTP_REQUEST_SLINE *s = (HTTP_REQUEST_SLINE *)l;
	if(s->URI)
		XFREE(MTYPE_STRING, s->URI);

	XFREE(MTYPE_HTTP_REQUEST_SLINE, s);
}

void HttpResponseSlineFree(void *l)
{
	HTTP_RESPONSE_SLINE *s = (HTTP_RESPONSE_SLINE *)l;
	XFREE(MTYPE_HTTP_RESPONSE_SLINE, s);
}

/*
	HTTP请求报文起始行解析
*/
HTTP_REQUEST_SLINE *HttpRequestStartLineParse(const char *data)
{
	char *suffix_ptr = NULL;
	char *startLinestr = NULL, *ptr1 = NULL, *ptr2 = NULL;	
	HTTP_REQUEST_SLINE *Line = NULL;
	//printf("%s\n", data);
	suffix_ptr = strstr(data, HTTP_HEADER_SUFFIX);
	if(NULL == suffix_ptr)
		return NULL;

	startLinestr = XCALLOC(MTYPE_STRING, suffix_ptr-data+3);
	memcpy(startLinestr, data, suffix_ptr-data+2);
	/*
		now startLine: www.baidu.com\r\n\0
	*/
	Line = XCALLOC(MTYPE_HTTP_REQUEST_SLINE, sizeof(HTTP_REQUEST_SLINE));
	
	ptr1 = ptr2 = startLinestr;

	ptr2 = strstr(ptr1, " ");	
	if(NULL == ptr2)
		goto ERROR;
	*ptr2++ = '\0';/*now ptr1 is method*/
	printf("method: %s\n", ptr1);
	memcpy(Line->Method, ptr1, strlen(ptr1));
	ptr1 = ptr2;
	
	ptr2 = strstr(ptr1, " ");
	if(NULL == ptr2)
		goto ERROR;	
	Line->URI = XCALLOC(MTYPE_STRING, ptr2 - ptr1 + 1);
	*ptr2++ = '\0';/*now ptr1 is URI*/
	printf("URI: %s\n", ptr1);
	memcpy(Line->URI, ptr1, strlen(ptr1));
	ptr1 = ptr2;
	
	ptr2 = strstr(ptr1, HTTP_HEADER_SUFFIX);
	if(NULL == ptr2)
		goto ERROR;	
	*ptr2 = '\0';
	printf("Http_v: %s\n", ptr1);
	memcpy(Line->Http_v, ptr1, strlen(ptr1));

	XFREE(MTYPE_STRING, startLinestr);
	return Line;
ERROR:
	if(startLinestr)
		XFREE(MTYPE_STRING, startLinestr);
	if(Line)
		HttpRequestSlineFree(Line);
	return NULL;
}

/*
	HTTP响应报文起始行解析
*/
HTTP_RESPONSE_SLINE *HttpResponseStartLineParse(const char *data)
{
	char *suffix_ptr = NULL;
	char *startLinestr = NULL, *ptr1 = NULL, *ptr2 = NULL;	
	HTTP_RESPONSE_SLINE *Line = NULL;
	//printf("%s\n", data);
	suffix_ptr = strstr(data, HTTP_HEADER_SUFFIX);
	if(NULL == suffix_ptr)
		return NULL;

	startLinestr = XCALLOC(MTYPE_STRING, suffix_ptr-data+3);
	memcpy(startLinestr, data, suffix_ptr-data+2);
	/*
		now startLine: HTTP/1.1 200 OK\r\n\0
	*/
	Line = XCALLOC(MTYPE_HTTP_RESPONSE_SLINE, sizeof(HTTP_RESPONSE_SLINE));
	
	ptr1 = ptr2 = startLinestr;

	ptr2 = strstr(ptr1, " ");	
	if(NULL == ptr2)
		goto ERROR;
	*ptr2++ = '\0';/*now ptr1 is method*/
	printf("Http_v: %s\n", ptr1);
	memcpy(Line->http_v, ptr1, strlen(ptr1));
	ptr1 = ptr2;
	
	ptr2 = strstr(ptr1, " ");
	if(NULL == ptr2)
		goto ERROR; 
	*ptr2++ = '\0';/*now ptr1 is URI*/
	printf("http_code: %s\n", ptr1);
	Line->http_code = atoi(ptr1);
	ptr1 = ptr2;
	
	ptr2 = strstr(ptr1, HTTP_HEADER_SUFFIX);
	if(NULL == ptr2)
		goto ERROR; 
	*ptr2 = '\0';
	printf("code_des: %s\n", ptr1);
	memcpy(Line->code_des, ptr1, strlen(ptr1));

	XFREE(MTYPE_STRING, startLinestr);
	return Line;
ERROR:
	if(startLinestr)
		XFREE(MTYPE_STRING, startLinestr);
	if(Line)
		HttpResponseSlineFree(Line);
	return NULL;
}


void GetHostIpPostFromHost(char *url, char *ipAddr, int *port)
{
	char *Port = NULL, *ptr = NULL;
	if(NULL == url)
		return;

	/*check whether port existed*/
	ptr = strstr(url, ":");
	if(NULL != ptr)
	{
		Port = ptr+1;
		memcpy(ipAddr, url, ptr-url);
		*port = atoi(Port);
	}
	else
	{
		memcpy(ipAddr, url, strlen(url));
		*port = DEFAULT_HTTP_PORT;
	}
	
	fprintf(stdout, "H:%s, P:%d\n", ipAddr ,*port);

}

int GetHostIpPostFromURI(char *URI, char *ipAddr, int *port)
{
	char *ptr = NULL;
	char HttpOrHttps[25] = {0};
	if(NULL == URI)
		return -1;

	printf("URI:%s\n", URI);

	char *URIDUP = XSTRDUP(MTYPE_STRING, URI);
	char *uriPtr = URIDUP;
	
	ptr = strstr(uriPtr, "://");
	if(ptr)
	{
		if(ptr - uriPtr >= sizeof(HttpOrHttps))
			return -1;
		memcpy(HttpOrHttps, uriPtr, ptr - uriPtr);
		if(STRCMP_EQUAL(HttpOrHttps, "http"))
			*port = DEFAULT_HTTP_PORT;
		else if(STRCMP_EQUAL(HttpOrHttps, "https"))
			*port = DEFAULT_HTTPS_PORT;
		else
			goto ERROR;

		uriPtr = ptr += 3;
	}

	/*check whether port existed*/
	ptr = strstr(uriPtr, ":");
	if(NULL != ptr)
	{
		memcpy(ipAddr, uriPtr, ptr-uriPtr);
		uriPtr = ptr+1;
		ptr = strstr(uriPtr, "/");
		if(ptr)
		{
			*ptr = '\0';
			*port = atoi(uriPtr);
		}
		else 
		{
			*port = atoi(uriPtr);			
		}
			
	}
	else
	{
		ptr = strstr(uriPtr, "/");
		if(ptr)
			memcpy(ipAddr, uriPtr, ptr-uriPtr);
		else 
			memcpy(ipAddr, uriPtr, strlen(uriPtr));			
	}

	fprintf(stdout, "H:%s, P:%d\n", ipAddr ,*port);

	XFREE(MTYPE_STRING, URIDUP);
	return 0;

ERROR:
	XFREE(MTYPE_STRING, URIDUP);
	return -1;
}

