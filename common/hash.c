#include "hash.h"
#include "memory.h"
#include "memtypes.h"
/*create hash table*/
struct hash *hash_create_size(unsigned int size, unsigned int (*hash_key)(void *),
                                     int (*hash_cmp)(const void *, const void *))
{
	struct hash *hash;

	hash = XMALLOC(MTYPE_HASH, sizeof(struct hash));
	hash->index = XCALLOC(MTYPE_HASH_INDEX,
		sizeof(struct hash_backet *)*size);
	hash->size = size;
	hash->hash_key = hash_key;
	hash->hash_cmp = hash_cmp;
	hash->count = 0;

	return hash;
}

/*create hash table by default hashtable size*/
struct hash *hash_create(unsigned int (*hash_key)(void *), 
             int (*hash_cmp)(const void *, const void *))
{
	return hash_create_size(HASHTABSIZE, hash_key, hash_cmp);
}

/* Utility function for hash_get().  When this function is specified
   as alloc_func, return arugment as it is.  This function is used for
   intern already allocated value.  */
void *hash_alloc_intern(void *arg)
{
	return arg;
}

/* Lookup and return hash backet in hash.  If there is no
   corresponding hash backet and alloc_func is specified, create new
   hash backet.  */
void *hash_get(struct hash *hash, void *data, void *(*alloc_func)(void *))
{
	unsigned int key;
	unsigned int index;
	void *newdata;
	struct hash_backet *backet;

	key = (*hash->hash_key) (data);
	index = key % hash->size;

	for (backet = hash->index[index]; backet != NULL; backet = backet->next) 
		if (backet->key == key && (*hash->hash_cmp) (backet->data, data))
			return backet->data;

	if (alloc_func)
	{
		newdata = (*alloc_func) (data);
		if (newdata == NULL)
			return NULL;

		backet = XMALLOC (MTYPE_HASH_BACKET, sizeof (struct hash_backet));
		backet->data = newdata;
		backet->key = key;
		backet->next = hash->index[index];
		hash->index[index] = backet;
		hash->count++;
		return backet->data;
	}
	return NULL;
}

/* Hash lookup.  */
void *hash_lookup(struct hash *hash, void *data)
{
	return hash_get (hash, data, NULL);
}

/* This function release registered value from specified hash.  When
   release is successfully finished, return the data pointer in the
   hash backet.  */
void *hash_release(struct hash *hash, void *data)
{
	void *ret;
	unsigned int key;
	unsigned int index;
	struct hash_backet *backet;
	struct hash_backet *pp;

	key = (*hash->hash_key) (data);
	index = key % hash->size;

	for (backet = pp = hash->index[index]; backet; backet = backet->next)
	{
		if (backet->key == key && (*hash->hash_cmp) (backet->data, data))
		{
			if (backet == pp) 
				hash->index[index] = backet->next;
			else 
				pp->next = backet->next;

			ret = backet->data;
			XFREE (MTYPE_HASH_BACKET, backet);
			hash->count--;
			return ret;
		}
		pp = backet;
	}
	return NULL;
}

/* Iterator function for hash.  */
void hash_iterate(struct hash *hash, 
	      void (*func)(struct hash_backet *, void *), void *arg)
{
	unsigned int i;
	struct hash_backet *hb;
	struct hash_backet *hbnext;

	for (i = 0; i < hash->size; i++)
	for (hb = hash->index[i]; hb; hb = hbnext)
	{
		/* get pointer to next hash backet here, in case (*func)
		* decides to delete hb by calling hash_release
		*/
		hbnext = hb->next;
		(*func) (hb, arg);
	}
}

/* Clean up hash.  */
void hash_clean(struct hash *hash, void (*free_func)(void *))
{
	unsigned int i;
	struct hash_backet *hb;
	struct hash_backet *next;

	for (i = 0; i < hash->size; i++)
	{
		for (hb = hash->index[i]; hb; hb = next)
		{
			next = hb->next;

			if (free_func)
				(*free_func) (hb->data);

			XFREE (MTYPE_HASH_BACKET, hb);
			hash->count--;
		}
		hash->index[i] = NULL;
	}
}

/* Free hash memory.  You may call hash_clean before call this
   function.  */
void hash_free(struct hash *hash)
{
	XFREE (MTYPE_HASH_INDEX, hash->index);
	XFREE (MTYPE_HASH, hash);
}

