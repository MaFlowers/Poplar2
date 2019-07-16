#ifndef __POP_HTTPCACHE_H__
#define __POP_HTTPCACHE_H__

/*cacheĬ�ϱ���Ŀ¼*/
#define DEFAULT_HTTP_CACHE_STORE_PATH	"/var/cache/poplar"

#define CACHE_DISK_STORE_SIZE_MAX		1024*8/*һ��������󱣴�8k��������*/

typedef struct{
	char *path;/*cache store path*/
	int length;/*the length of path*/
}CacheRoot;

/*����*/
void CacheRootSetStorePath(char *path);
void CacheRootDestroy();
int CacheDiskStore(char *URI, int URI_len, unsigned char *data, unsigned int data_len);
int CacheDiskExistCheckPath(char *CachePath);
int CacheDiskExistCheckUri(char *URI, int URI_len);
unsigned int CacheDiskGet(char *URI, int URI_len, unsigned char *out_buf, int buflen);

#endif
