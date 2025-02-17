/* This code is written by Nathan Jones,
Hash function for strings is inspired from
djb2 by Dan Bernstein, taken from 
Stack Overflow
*/

#include <stdio.h>  
#include <stdlib.h>
#include <string.h>
#include "strmap.h"

// djb2 hash function
unsigned int hash_string(char *str, unsigned int nbuckets) {
    unsigned long hash = 0;
    while (*str) {
        hash = ((hash << 5) - hash) + *str;
        str++;
    }
    return hash % nbuckets;
}

/* Create a new hashtab, initialized to empty. */
strmap_t *strmap_create(int numbuckets) {
    if (numbuckets < MIN_BUCKETS || numbuckets > MAX_BUCKETS) {
        return NULL;
    }

    strmap_t *map = malloc(sizeof(strmap_t));
    if (!map) {
        return NULL;
    }

    map->strmap_buckets = calloc(numbuckets, sizeof(smel_t*));
    if (!map->strmap_buckets) {
        free(map);
        return NULL;
    }
    
    map->strmap_size = 0;
    map->strmap_nbuckets = numbuckets;
    return map;
}

/* Insert an element with the given key and value.
 *  Return the previous value associated with that key, or null if none.
 */
void *strmap_put(strmap_t *m, char *key, void *value) {
    if (!m || !key) {
        return NULL;
    } 
    
    unsigned int bucket = hash_string(key, m->strmap_nbuckets);
    smel_t *current = m->strmap_buckets[bucket];
    smel_t *prev = NULL;
    
    // Traverse to find correct position in sorted order
    while (current && strcmp(current->sme_key, key) < 0) {
        prev = current;
        current = current->sme_next;
    }
    
    // Check if key already exists
    if (current && strcmp(current->sme_key, key) == 0) {
        void *old_value = current->sme_value;
        current->sme_value = value;
        return old_value;
    }
    
    // Create new element
    smel_t *new_elem = malloc(sizeof(smel_t));
    if (!new_elem) {
        return NULL;
    }
    
    new_elem->sme_key = strdup(key);
    new_elem->sme_value = value;
    new_elem->sme_next = current;
    
    // Insert in sorted position
    if (prev) {
        prev->sme_next = new_elem;
    } else {
        m->strmap_buckets[bucket] = new_elem;
    }
    
    m->strmap_size++;
    return NULL;
}

/* return the value associated with the given key, or null if none */
void *strmap_get(strmap_t *m, char *key) {
    if (!m || !key) {
        return NULL;
    }

    unsigned int bucket = hash_string(key, m->strmap_nbuckets);
    smel_t *current = m->strmap_buckets[bucket];
    
    while (current) {
        if (strcmp(current->sme_key, key) == 0) {
            return current->sme_value;
        }
        current = current->sme_next;
    }
    
    return NULL;
}

/* remove the element with the given key and return its value.
   Return null if the hashtab contains no element with the given key */
void *strmap_remove(strmap_t *m, char *key) {
    if (!m || !key) return NULL;
    unsigned int bucket = hash_string(key, m->strmap_nbuckets);
    smel_t *current = m->strmap_buckets[bucket];
    smel_t *prev = NULL;
    
    while (current) {
        if (strcmp(current->sme_key, key) == 0) {
            void *value = current->sme_value;
            if (prev) {
                prev->sme_next = current->sme_next;
            } else {
                m->strmap_buckets[bucket] = current->sme_next;
            }
            free(current->sme_key);
            free(current);
            m->strmap_size--;
            return value;
        }
        prev = current;
        current = current->sme_next;
    }
    
    return NULL;
}

/* return the # of elements in the hashtab */
int strmap_getsize(strmap_t *m) {
    return m ? m->strmap_size : 0;      // returns 0 if m is NULL
}

/* return the # of buckets in the hashtab */
int strmap_getnbuckets(strmap_t *m) {
    return m ? m->strmap_nbuckets : 0;  // returns 0 if m is NULL
}
/* print out the contents of each bucket */
void strmap_dump(strmap_t *m) {
    if (!m) return;
    
    printf("total elements = %d.\n", m->strmap_size);
    for (unsigned int i = 0; i < m->strmap_nbuckets; i++) {
        smel_t *current = m->strmap_buckets[i];
        if (current) { 
            printf("bucket %u:\n", i);
            while (current) {
                printf("  %s->%p\n", current->sme_key, current->sme_value);
                current = current->sme_next;
            }
        }
    }
}