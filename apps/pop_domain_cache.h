#ifndef __POP_DOMAIN_CACHE_H__
#define __POP_DOMAIN_CACHE_H__

#define DNS_CACHE_VALIDITY_CHECK_PERIOD			60

#define DNS_CACHE_RECORD_COUNTS_MAX				1000

typedef struct{
	unsigned short TransactionID;/*��ʶ��ʾ��Ӧ�����еı�ʶ*/
	char *HostName;
	unsigned int Address;
	unsigned int TimeLive;
	unsigned int ValidityEndTime;/*��Ч����ʱ��*/
}DnsQueryResult;

/*
	DNS������DNS��������е�DNS������ͬ��
*/

/*DNS��������ڿ��ٲ�ѯ������Ӧ�ĵ�ַ*/
typedef struct hash DnsCacheMap;
/*DNS������У����ڶ��ڼ��DNS��Ч��*/
typedef struct list DnsCacheQueue;

typedef struct
{
	DnsCacheMap *Map;/*DNS��������ڿ��ٲ��һ���*/
	/*
		DNS������У����水�������ʶ����У����ʶ�Խ�ߣ�Խ����
		����µ�DNS������ӵ�����β�������л���󣬽������Ƶ�����β
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
