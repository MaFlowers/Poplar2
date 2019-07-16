#include "pop_domain_cache.h"
#include "hash.h"
#include "list.h"
#include "memory.h"
#include "popd.h"
#include "hash_key.h"
#include "pop_socket.h"
#include <string.h>
#include <assert.h>
static struct event_node *t_DnsCacheValidityCheck = NULL;

/*缓存对象接口*/
DnsQueryResult *DnsQueryResultNew()
{
	return (DnsQueryResult *)XCALLOC(MTYPE_DNS_RESULT, sizeof(DnsQueryResult));
}

void DnsQueryResultFree(void *p)
{
	DnsQueryResult *R = (DnsQueryResult *)p;
	if(R->HostName)
		XFREE(MTYPE_STRING, R->HostName);

	XFREE(MTYPE_DNS_RESULT, R);
}

/*
	缓存对照表接口
*/

static unsigned int DnsCacheMapGetKey(void *p)
{
	DnsQueryResult *R = (DnsQueryResult *)p;
	assert(R->HostName);
	return BKDRHash(R->HostName);
}

/*
	返回值
		相同: 0
		不同: 不为0
*/
static int DnsCacheMapCmp(const void *p1, const void *p2)
{
	assert(NULL != p1 && NULL != p2);
	DnsQueryResult *R1 = (DnsQueryResult *)p1;
	DnsQueryResult *R2 = (DnsQueryResult *)p2;
	int len1 = strlen(R1->HostName);
	int len2 = strlen(R2->HostName);

	if(len1 != len2)
		return 0;

	if(memcmp(R1->HostName, R2->HostName, len1) != 0)
		return 0;

	return 1;
}

/*创建DNS缓冲表*/
DnsCacheMap *DnsCacheMapCreate()
{
	return (DnsCacheMap *)hash_create(DnsCacheMapGetKey, DnsCacheMapCmp);
}

/*创建DNS缓冲表*/
DnsCacheMap *DnsCacheMapCreateSize(int size)
{
	return hash_create_size(size, DnsCacheMapGetKey, DnsCacheMapCmp);
}

/*清空DNS缓冲表*/
void DnsCacheMapClean(DnsCacheMap *M)
{
	hash_clean((struct hash *)M, NULL);
}

/*释放DNS缓冲表*/
void DnsCacheMapDestroy(DnsCacheMap *M)
{
	hash_clean((struct hash *)M, NULL);
	hash_free((struct hash *)M);
}

static void* DnsCacheMapRef(void *data)
{
	return data;
}

/*添加DNS缓存到CacheMap*/
DnsQueryResult *DnsCacheMapAdd(DnsCacheMap *M, DnsQueryResult *p)
{
	return (DnsQueryResult *)hash_get((struct hash *)M, p, DnsCacheMapRef);
}

/*从CacheMap中删除DNS缓存*/
DnsQueryResult *DnsCacheMapDel(DnsCacheMap *M,  DnsQueryResult *p)
{
	return (DnsQueryResult *)hash_release((struct hash *)M, p);
}

/*从CacheMap中通过域名查找DNS缓存*/
DnsQueryResult *DnsCacheMapLookupByHostname(DnsCacheMap *M, char *hostname)
{
	unsigned int key, index;
	struct hash_backet *backet = NULL;
	DnsQueryResult *R = NULL;
	struct hash *hash = (DnsCacheMap *)M;
	int len1, len2;

	key = BKDRHash(hostname);
	index = key % hash->size;

	len1 = strlen(hostname);
	for(backet = hash->index[index]; backet != NULL; backet = backet->next)
	{
		R = (DnsQueryResult *)backet->data;
		len2 = strlen(R->HostName);

		if(len1 != len2)
			continue;

		if(memcmp(R->HostName, hostname, len1) != 0)
			continue;

		return R;
	}	

	return NULL;
}

/*
	DNS缓存队列
*/
DnsCacheQueue *DnsCacheQueueCreate()
{
	return (DnsCacheQueue *)list_new();
}

/*清空队列*/
void DnsCacheQueueClean(DnsCacheQueue *F)
{
	list_delete_all_node((struct list *)F);
}

void DnsCacheQueueDestroy(DnsCacheQueue *F)
{
	list_delete((struct list *)F);
}

/*添加DNS缓存到队列最后*/
void DnsCacheQueueAdd(DnsCacheQueue *M, DnsQueryResult *p)
{
	listnode_add((struct list *)M, p);
}
/*从队列中删除DNS缓存*/
void DnsCacheQueueDel(DnsCacheQueue *M, DnsQueryResult *p)
{
	listnode_delete((struct list *)M, p);
}


/*
	DNS缓存管理模块
	DNS缓存表和DNS队列都保存一份DNS缓存，并且其中的缓存是同步的
	DNS缓存需要处理过期缓存，设置定时器来处理
*/
DnsCache *DnsCacheCreate()
{
	DnsCache *Cache = XCALLOC(MTYPE_DNS_CACHE, sizeof(DnsCache));
	Cache->Map = DnsCacheMapCreate();
	Cache->Queue = DnsCacheQueueCreate();

	return Cache;
}

DnsCache *DnsCacheCreateSize(int size)
{
	DnsCache *Cache = XCALLOC(MTYPE_DNS_CACHE, sizeof(DnsCache));
	Cache->Map = DnsCacheMapCreateSize(size);
	Cache->Queue = DnsCacheQueueCreate();

	return Cache;
}

void DnsCacheDestroy(DnsCache *Cache)
{
	DnsCacheMapDestroy(Cache->Map);
	Cache->Queue->del = DnsQueryResultFree;/*释放队列时，顺便释放缓存*/
	DnsCacheQueueDestroy(Cache->Queue);
	XFREE(MTYPE_DNS_CACHE, Cache);
}

void DnsCacheAdd(DnsCache *Cache, DnsQueryResult *p)
{
	DnsCacheMapAdd(Cache->Map, p);
	DnsCacheQueueAdd(Cache->Queue, p);/*新的缓存添加到队列尾*/
}

void DnsCacheDel(DnsCache *Cache, DnsQueryResult *p)
{
	DnsCacheMapDel(Cache->Map, p);
	DnsCacheQueueDel(Cache->Queue, p);	
	DnsQueryResultFree(p);
}

void DnsCacheFreshnessUpdate(DnsCache *Cache, DnsQueryResult *p)
{
	if(listgetdata(Cache->Queue->tail) == p)
		return;

	DnsCacheQueueDel(Cache->Queue, p);		
	DnsCacheQueueAdd(Cache->Queue, p);/*新的缓存添加到队列尾*/	
}

void DnsCacheClean(DnsCache *Cache, DnsQueryResult *p)
{
	DnsCacheMapClean(Cache->Map);
	Cache->Queue->del = DnsQueryResultFree;/*清空队列时，顺便释放缓存*/
	DnsCacheQueueClean(Cache->Queue);	
	Cache->Queue->del = NULL;
}

/*通过域名查找DNS缓存*/
DnsQueryResult *DnsCacheLookupByHostname(DnsCache *Cache, char *hostname)
{
	return DnsCacheMapLookupByHostname(Cache->Map, hostname);
}

/*获取DNS缓存数量*/
inline int DnsCacheRecordCounts(DnsCache *Cache)
{
	return listcount(Cache->Queue);
}

/*
	DNS缓存检查是否超出阈值
	返回值： 
		1 ———— 超出阈值
		0 ———— 未超出阈值
	*/
int DnsCacheRecordCountsCheck(DnsCache *Cache)
{
	if(DnsCacheRecordCounts(Cache) > DNS_CACHE_RECORD_COUNTS_MAX)
		return 1;
	else
		return 0;
}

/*
	缓存最多保存1000条
	在添加新的缓存时，若当前缓存量超出阈值，
	则删除阈值的1/3，且按新鲜度从低到高删除
*/
int DnsCacheCheckAndDelete(DnsCache *Cache)
{
	struct listnode *nn = NULL, *nm = NULL;
	DnsQueryResult *R = NULL;
	int limit = DNS_CACHE_RECORD_COUNTS_MAX/3;
	if(DnsCacheRecordCountsCheck(Cache))
	{
		int count = 0;
		for(ALL_LIST_ELEMENTS(Cache->Queue, nn, nm, R))
		{
			if(count >= limit)
				break;

			DnsCacheDel(Cache, R);
			count++;
		}
	}

	return 0;
}

/*定期清理过期DNS缓存*/
void DnsCacheValidityCheckTimeout(struct event_node *node)
{
	DnsQueryResult *R = NULL;
	DnsCache *Cache = EVENT_ARG(node);
	struct listnode *nn = NULL, *nm = NULL;
	t_DnsCacheValidityCheck = NULL;
	fprintf(stdout, "DnsCache Time out .\n");
	unsigned int currentTime = time(NULL);

	for(ALL_LIST_ELEMENTS(Cache->Queue, nn, nm, R))
	{
		if(currentTime >= R->ValidityEndTime)
		{/*DNS Cache到期*/
			DnsCacheDel(Cache, R);
		}
	}
	DnsCacheValidityCheckTimerSet(Cache);
}

void DnsCacheValidityCheckTimerSet(DnsCache *Cache)
{
	TIMER_ON(t_DnsCacheValidityCheck, DnsCacheValidityCheckTimeout, Cache, DNS_CACHE_VALIDITY_CHECK_PERIOD);
}

void DnsCacheValidityCheckTimerCacel()
{
	TIMER_OFF(t_DnsCacheValidityCheck);
}

