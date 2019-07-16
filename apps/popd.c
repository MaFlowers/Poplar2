#include "popd.h"
#include "pop_network.h"
#include "pop_domain_parse.h"
#include "pop_domain_cache.h"
#include "pop_httpcache.h"
int pop_init()
{
	popd->http_proxy_conf.sockfd = pop_http_proxy_tcp_socket(NULL, POP_HTTP_PROXY_SERVER_DEFAULT_PORT);
	if(popd->http_proxy_conf.sockfd < 0)
		goto ERROR;
	popd->http_proxy_conf.paired_peer_tcp_list = list_new();
	popd->http_proxy_conf.paired_peer_tcp_list->del = (void (*)(void *))pop_peer_delete;
	popd->http_proxy_conf.paired_peer_hash_table = pop_paired_peer_hash_table_init();//����ʹ����

	/*��ʼ��DNSģ��*/
	popd->http_proxy_conf.dns_client_peer = pop_domain_parse_peer_init("114.114.114.114", 53);
	if(NULL == popd->http_proxy_conf.dns_client_peer)
		goto ERROR;

	popd->http_proxy_conf.dns_ack_wait_queue = DnsWaitAckQueueCreate();
	if(NULL == popd->http_proxy_conf.dns_ack_wait_queue)
		goto ERROR;

	popd->http_proxy_conf.dns_cache = DnsCacheCreate();
	if(NULL == popd->http_proxy_conf.dns_cache)
		goto ERROR;

	/*����DNS������Ч�Լ�鶨ʱ��*/
	DnsCacheValidityCheckTimerSet(popd->http_proxy_conf.dns_cache);

	/*����HTTP����*/
	CacheRootSetStorePath(NULL);
	return 0;
	
ERROR:
	exit(1);
}



/*
	��ӡ�ֽ�������16������ʾ
*/
void _debug_show_xbyte(unsigned char *src_buf, int len, const char *func_str, int code_line)
{
	char temp_buf[5120];
	char str_buf[8];
	int i;

	snprintf(temp_buf,sizeof(temp_buf), "<%s,%d>", func_str, code_line);
	
	for(i = 0; i < len; i++)
	{
        if(0 == (i%8))
            strcat(temp_buf, "    ");
            
		if(0 == (i%16))
			strcat(temp_buf, "\r\n");
		
		sprintf(str_buf, "%02x ", src_buf[i]);
		strcat(temp_buf, str_buf);		
	}

	printf("%s\n", temp_buf);
}



static char _xbytes[1024];
/*
	���ֽ�����ת������16��������
*/
char * _bytes_to_x(unsigned char *src_buf, int len)
{
	char str_buf[8];
	int i;

	_xbytes[0] = '\0';
	for(i = 0; i < len; i++)
	{
		if((0 != i) && (0 == (i%8)))
			strcat(_xbytes, "  ");
		
		sprintf(str_buf, "%02x ", src_buf[i]);
		strcat(_xbytes, str_buf);		
	}

	return _xbytes;
}



