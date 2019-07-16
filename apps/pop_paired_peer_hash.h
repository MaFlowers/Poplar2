#ifndef __POP_PAIRED_PEER_HASH_H__
#define __POP_PAIRED_PEER_HASH_H__
#include "pop_paired_peer.h"
#include "hash.h"

#define POP_PAIRED_SOCK_HASH_TABLE_SIZE			1024

struct peer * pop_paired_peer_hash_lookup_by_sockself(struct hash *hash, unsigned int sockfd);
int pop_paired_peer_hash_add(struct hash *hash, struct peer *p1, struct peer *p2);
int pop_paired_peer_hash_del(struct hash *hash, struct peer *p1, struct peer *p2);
struct hash *pop_paired_peer_hash_table_init(void);

#endif
