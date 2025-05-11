/* This code is written by Nathan Jones,
Hash function for strings is inspired from
djb2 by Dan Bernstein, taken from 
Stack Overflow
*/

#include <stdio.h>  
#include <stdlib.h>
#include <string.h>
#include "strmapbis.h"

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
    // Clamp numbuckets to valid range
    if (numbuckets < MIN_BUCKETS) {
        numbuckets = MIN_BUCKETS;
    } else if (numbuckets > MAX_BUCKETS) {
        numbuckets = MAX_BUCKETS;
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
    
    // Check if key already exists
    while (current) {
        if (strcmp(current->sme_key, key) == 0) {
            void *old_value = current->sme_value; // Store old value
            current->sme_value = value; // Update with new value
            return old_value; // Return old value
        }
        prev = current;
        current = current->sme_next;
    }
    
    // Create new element
    smel_t *new_elem = malloc(sizeof(smel_t));
    if (!new_elem) {
        return NULL;  // Allocation failure
    }
    
    new_elem->sme_key = strdup(key);
    if (!new_elem->sme_key) {
        free(new_elem);
        return NULL;  // strdup failure
    }
    
    new_elem->sme_value = value;
    new_elem->sme_next = NULL;
    
    // Insert at the head of the bucket (or sorted position)
    if (prev) {
        prev->sme_next = new_elem;
    } else {
        m->strmap_buckets[bucket] = new_elem;
    }
    
    m->strmap_size++;
    return NULL; // No previous value
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

/* return the current load factor of the map  */
double strmap_getloadfactor(strmap_t *m) {
    if (m == NULL || m->strmap_nbuckets == 0) {
        return 0.0;  // Return 0 for invalid map or to avoid division by zero
    }
    
    // Calculate load factor
    return (double)m->strmap_size / (double)m->strmap_nbuckets;
}

/* if the load factor is more than LFSLOP away from the target,
 * allocate a new array to bring the load factor within 1/8 of
 * the given target (i.e., between 7/8*target and 9/8*target),
 * then re-hash everything into that array, free the old bucket
 * array, and make the map point to the new bucket array.  (The only
 * fields that change in the map are strmap_buckets and strmap_nbuckets.)
 * If the load factor is already within the target range,
 * the map is left unchanged. 
 */
void strmap_resize(strmap_t *m, double targetLF) {
    if (!m) return;
    
    // Get current load factor
    double currentLF = strmap_getloadfactor(m);
    
    // Check if we're already within the acceptable range
    if (currentLF >= targetLF * (1 - LFSLOP) && 
        currentLF <= targetLF * (1 + LFSLOP)) {
        return;  // Current load factor is acceptable
    }
    
    // Calculate new number of buckets needed
    int newBuckets = (int)(m->strmap_size / targetLF);
    
    // Ensure new bucket count is within allowed range
    if (newBuckets < MIN_BUCKETS) {
        newBuckets = MIN_BUCKETS;
    } else if (newBuckets > MAX_BUCKETS) {
        newBuckets = MAX_BUCKETS;
    }
    
    // If new bucket count is same as current, no need to resize
    if (newBuckets == m->strmap_nbuckets) {
        return;
    }
    
    // Allocate new bucket array
    smel_t **newBucketsArray = calloc(newBuckets, sizeof(smel_t *));
    if (!newBucketsArray) {
        return;  // Memory allocation failed
    }
    
    // Rehash all existing elements into new bucket array
    for (unsigned int i = 0; i < m->strmap_nbuckets; i++) {
        smel_t *current = m->strmap_buckets[i];
        while (current != NULL) {
            // Save next pointer before we modify current
            smel_t *next = current->sme_next;
            
            // Calculate new bucket for this element
            unsigned int newIndex = hash_string(current->sme_key, newBuckets);
            
            // Insert at head of new bucket
            current->sme_next = newBucketsArray[newIndex];
            newBucketsArray[newIndex] = current;
            
            // Move to next element
            current = next;
        }
    }
    
    // Free old bucket array
    free(m->strmap_buckets);
    
    // Update map to use new bucket array
    m->strmap_buckets = newBucketsArray;
    m->strmap_nbuckets = newBuckets;
}