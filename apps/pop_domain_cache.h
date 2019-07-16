#ifndef __POP_DOMAIN_CACHE_H__
#define __POP_DOMAIN_CACHE_H__

#define DNS_CACHE_VALIDITY_CHECK_PERIOD			60

#define DNS_CACHE_RECORD_COUNTS_MAX				1000

typedef struct{
	unsigned short TransactionID;/*标识表示回应报文中的标识*/
	char *HostName;
	unsigned int Address;
	unsigned int TimeLive;
	unsigned int ValidityEndTime;/*有效截至时间*/
}DnsQueryResult;

/*
	DNS缓存表和DNS缓存队列中的DNS缓存是同步
*/

/*DNS缓存表，用于快速查询域名对应的地址*/
typedef struct hash DnsCacheMap;
/*DNS缓存队列，用于定期检查DNS有效性*/
typedef struct list DnsCacheQueue;

typedef struct
{
	DnsCacheMap *Map;/*DNS缓存表，用于快速查找缓存*/
	/*
		DNS缓存队列，缓存按命中新鲜度排列，新鲜度越高，越靠后
		因此新的DNS缓存添加到队列尾，在命中缓存后，将缓存移到队列尾
	*/
	DnsCacheQueue *Queue;
}DnsCache;

DnsQueryResult *DnsQueryResultNew();
void DnsQueryResultFree(void *p);

DnsCache *DnsCacheCreate();
DnsCache *DnsCacheCreateSize(int size);
void DnsCacheDestroy(DnsCache *Cache);
void DnsCacheAdd(DnsCache *Cache, DnsQueryResult *p);
void DnsCacheDel(DnsCache *Cache, DnsQueryResult *p);
void DnsCacheFreshnessUpdate(DnsCache *Cache, DnsQueryResult *p);
void DnsCacheClean(DnsCache *Cache, DnsQueryResult *p);
DnsQueryResult *DnsCacheLookupByHostname(DnsCache *Cache, char *hostname);
void DnsCacheValidityCheckTimerSet(DnsCache *Cache);
void DnsCacheValidityCheckTimerCacel();
int DnsCacheRecordCountsCheck(DnsCache *Cache);
int DnsCacheCheckAndDelete(DnsCache *Cache);

#endif
