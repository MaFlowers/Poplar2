#ifndef __POP_HTTPCACHE_H__
#define __POP_HTTPCACHE_H__

/*cache默认保存目录*/
#define DEFAULT_HTTP_CACHE_STORE_PATH	"/var/cache/poplar"

#define CACHE_DISK_STORE_SIZE_MAX		1024*8/*一个缓存最大保存8k，否则丢弃*/

typedef struct{
	char *path;/*cache store path*/
	int length;/*the length of path*/
}CacheRoot;

/*保存*/
void CacheRootSetStorePath(char *path);
void CacheRootDestroy();
int CacheDiskStore(char *URI, int URI_len, unsigned char *data, unsigned int data_len);
int CacheDiskExistCheckPath(char *CachePath);
int CacheDiskExistCheckUri(char *URI, int URI_len);
unsigned int CacheDiskGet(char *URI, int URI_len, unsigned char *out_buf, int buflen);

#endif
