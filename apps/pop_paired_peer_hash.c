#include "pop_paired_peer_hash.h"
#include "popd.h"
#include "memory.h"
#include "memtypes.h"

static unsigned int pop_paired_peer_hash_key(void *p)
{
	struct peer *pair = (struct peer *)p;
	return pair->sockfd;/*��ӵ���ϣ��ʱ��ȡ��*/
}

static int pop_paired_peer_hash_cmp(const void *p1, const void *p2)
{
	struct peer *pair1, *pair2;
	if(NULL == p1 || NULL == p2)
	{
		fprintf(stdout, "<%s, %d>warning: compare sock pairs, p1: %p, p2: %p\n", __FUNCTION__, __LINE__, p1, p2);
		return 0;
	}

	pair1 = (struct peer *)p1;
	pair2 = (struct peer *)p2;

	if(pair1->sockfd != pair2->sockfd)
		return 0;

	if(pair1->paired_peer != pair2->paired_peer)
		return 0;

	return 1;
}

/*��ʼ����ϣ��*/
struct hash *pop_paired_peer_hash_table_init(void)
{
	struct hash *hash = hash_create_size (POP_PAIRED_SOCK_HASH_TABLE_SIZE, pop_paired_peer_hash_key,
										pop_paired_peer_hash_cmp);
	if(NULL == hash)
	{
		fprintf(stderr, "Init paired sock hash table error.\n");
		return NULL;
	}
	return hash;
}

static void* pop_paired_peer_hash_ref(void *data)
{
	return data;
}

/*ͨ������SOCKET����*/
struct peer * pop_paired_peer_hash_lookup_by_sockself(struct hash *hash, unsigned int sockfd)
{
	unsigned int key, index;
	struct hash_backet *backet = NULL;
	struct peer *pair = NULL;

	key = sockfd;
	index = key % hash->size;

	for(backet = hash->index[index]; backet != NULL; backet = backet->next)
	{
		if(backet->key == key)
		{
			pair = (struct peer *)backet->data;
			return pair;
		}
	}

	return NULL;
}

/*���ϣ�������һ���׽���*/
static int pop_paired_peer_hash_add_one(struct hash *hash, struct peer *p)
{
	if(p->sockfd < 0 || NULL == p->paired_peer || hash == NULL)
	{
		fprintf(stderr, "Add paired sock hash table failed, sockfd: %d\n", p->sockfd);
		return -1;
	}

	/*�����ϣ����*/
	hash_get(hash, p, pop_paired_peer_hash_ref);

	return 0;
}

/*�ӹ�ϣ����ɾ��һ���׽��ֶԣ����ͷ�*/
static void pop_paired_peer_hash_del_one(struct hash *hash, struct peer *p)
{
	assert(p);
	struct peer *tmp = NULL;
	tmp = hash_lookup(hash, p);
	if(tmp)
	{
		hash_release(hash, tmp);
		pop_peer_delete(tmp);
	}
	else
	{
		fprintf(stderr, "Can not find paired sock from hash, delete sockpair (self:%d) failed.\n", 
			p->sockfd);
	}
}

/*�����������׽��ֱ���ɶ���ӵ���ϣ*/
int pop_paired_peer_hash_add(struct hash *hash, struct peer *p1, struct peer *p2)
{
	int ret = -1;

	if(NULL == hash)
	{
		fprintf(stderr, "<%s, %d>hash is null.\n", __FUNCTION__, __LINE__);
		return -1;
	}
	
	if(NULL == p1 || NULL == p2
		|| NULL == p1->paired_peer || NULL == p2->paired_peer
		|| p1->paired_peer != p2 || p2->paired_peer != p1)
	{/*��ӵ������׽��ֱ��뻥�����*/
		fprintf(stderr, "<%s, %d>Add paired sock hash table failed.\n", __FUNCTION__, __LINE__);
		return -1;
	}

	ret = pop_paired_peer_hash_add_one(hash, p1);
	if(ret)
	{
		return -1;
	}
	
	ret = pop_paired_peer_hash_add_one(hash, p2);
	if(ret)
	{
		pop_paired_peer_hash_del_one(hash, p1);
		return -1;
	}

	return 0;
}

/*ɾ��һ���׽���ʱ������ɾ����һ��*/
int pop_paired_peer_hash_del(struct hash *hash, struct peer *p1, struct peer *p2)
{
	if(NULL == hash)
	{
		fprintf(stderr, "<%s, %d>hash is null.\n", __FUNCTION__, __LINE__);
		return -1;
	}

	if(NULL == p1 || NULL == p2
		|| NULL == p1->paired_peer || NULL == p2->paired_peer
		|| p1->paired_peer != p2 || p2->paired_peer != p1)
	{/*��ӵ������׽��ֱ��뻥�����*/
		fprintf(stderr, "Del paired sock hash table failed.");
		return -1;
	}

	pop_paired_peer_hash_del_one(hash, p1);
	pop_paired_peer_hash_del_one(hash, p2);

	return 0;
}

