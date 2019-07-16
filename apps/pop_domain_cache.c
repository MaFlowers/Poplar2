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

/*�������ӿ�*/
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
	������ձ�ӿ�
*/

static unsigned int DnsCacheMapGetKey(void *p)
{
	DnsQueryResult *R = (DnsQueryResult *)p;
	assert(R->HostName);
	return BKDRHash(R->HostName);
}

/*
	����ֵ
		��ͬ: 0
		��ͬ: ��Ϊ0
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

/*����DNS�����*/
DnsCacheMap *DnsCacheMapCreate()
{
	return (DnsCacheMap *)hash_create(DnsCacheMapGetKey, DnsCacheMapCmp);
}

/*����DNS�����*/
DnsCacheMap *DnsCacheMapCreateSize(int size)
{
	return hash_create_size(size, DnsCacheMapGetKey, DnsCacheMapCmp);
}

/*���DNS�����*/
void DnsCacheMapClean(DnsCacheMap *M)
{
	hash_clean((struct hash *)M, NULL);
}

/*�ͷ�DNS�����*/
void DnsCacheMapDestroy(DnsCacheMap *M)
{
	hash_clean((struct hash *)M, NULL);
	hash_free((struct hash *)M);
}

static void* DnsCacheMapRef(void *data)
{
	return data;
}

/*���DNS���浽CacheMap*/
DnsQueryResult *DnsCacheMapAdd(DnsCacheMap *M, DnsQueryResult *p)
{
	return (DnsQueryResult *)hash_get((struct hash *)M, p, DnsCacheMapRef);
}

/*��CacheMap��ɾ��DNS����*/
DnsQueryResult *DnsCacheMapDel(DnsCacheMap *M,  DnsQueryResult *p)
{
	return (DnsQueryResult *)hash_release((struct hash *)M, p);
}

/*��CacheMap��ͨ����������DNS����*/
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
	DNS�������
*/
DnsCacheQueue *DnsCacheQueueCreate()
{
	return (DnsCacheQueue *)list_new();
}

/*��ն���*/
void DnsCacheQueueClean(DnsCacheQueue *F)
{
	list_delete_all_node((struct list *)F);
}

void DnsCacheQueueDestroy(DnsCacheQueue *F)
{
	list_delete((struct list *)F);
}

/*���DNS���浽�������*/
void DnsCacheQueueAdd(DnsCacheQueue *M, DnsQueryResult *p)
{
	listnode_add((struct list *)M, p);
}
/*�Ӷ�����ɾ��DNS����*/
void DnsCacheQueueDel(DnsCacheQueue *M, DnsQueryResult *p)
{
	listnode_delete((struct list *)M, p);
}


/*
	DNS�������ģ��
	DNS������DNS���ж�����һ��DNS���棬�������еĻ�����ͬ����
	DNS������Ҫ������ڻ��棬���ö�ʱ��������
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
	Cache->Queue->del = DnsQueryResultFree;/*�ͷŶ���ʱ��˳���ͷŻ���*/
	DnsCacheQueueDestroy(Cache->Queue);
	XFREE(MTYPE_DNS_CACHE, Cache);
}

void DnsCacheAdd(DnsCache *Cache, DnsQueryResult *p)
{
	DnsCacheMapAdd(Cache->Map, p);
	DnsCacheQueueAdd(Cache->Queue, p);/*�µĻ�����ӵ�����β*/
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
	DnsCacheQueueAdd(Cache->Queue, p);/*�µĻ�����ӵ�����β*/	
}

void DnsCacheClean(DnsCache *Cache, DnsQueryResult *p)
{
	DnsCacheMapClean(Cache->Map);
	Cache->Queue->del = DnsQueryResultFree;/*��ն���ʱ��˳���ͷŻ���*/
	DnsCacheQueueClean(Cache->Queue);	
	Cache->Queue->del = NULL;
}

/*ͨ����������DNS����*/
DnsQueryResult *DnsCacheLookupByHostname(DnsCache *Cache, char *hostname)
{
	return DnsCacheMapLookupByHostname(Cache->Map, hostname);
}

/*��ȡDNS��������*/
inline int DnsCacheRecordCounts(DnsCache *Cache)
{
	return listcount(Cache->Queue);
}

/*
	DNS�������Ƿ񳬳���ֵ
	����ֵ�� 
		1 �������� ������ֵ
		0 �������� δ������ֵ
	*/
int DnsCacheRecordCountsCheck(DnsCache *Cache)
{
	if(DnsCacheRecordCounts(Cache) > DNS_CACHE_RECORD_COUNTS_MAX)
		return 1;
	else
		return 0;
}

/*
	������ౣ��1000��
	������µĻ���ʱ������ǰ������������ֵ��
	��ɾ����ֵ��1/3���Ұ����ʶȴӵ͵���ɾ��
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

/*�����������DNS����*/
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
		{/*DNS Cache����*/
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

