// This project is by Nathan Jones

#include <stdlib.h>
#include <string.h>
#include "cachesim.h"

// Helper function to get set index from address
static inline int get_set_index(addr_t addr, int s) {
    return (addr >> LOGBSIZE) & ((1 << s) - 1);
}

// Helper function to get tag from address
static inline addr_t get_tag(addr_t addr, int s) {
    return addr >> (LOGBSIZE + s);
}

// LRU information structure
typedef struct {
    int *lru_order;  // Array holding line indices in LRU order
} lru_info_t;

// Helper function to update LRU order
static void update_lru(lru_info_t *info, int accessed_idx, int E) {
    // Find current position of accessed index
    int pos = 0;
    while (pos < E && info->lru_order[pos] != accessed_idx) pos++;
    
    // Shift elements to make room for moving accessed_idx to end (most recently used)
    for (int i = pos; i < E - 1; i++) {
        info->lru_order[i] = info->lru_order[i + 1];
    }
    info->lru_order[E - 1] = accessed_idx;
}

// Helper function to get victim line index
static int get_lru_victim(lru_info_t *info) {
    return info->lru_order[0];  // Return least recently used line index
}

cache_t *cache_create(int s, int E, int delay, cache_t *nextlevel) {
    cache_t *cache = malloc(sizeof(cache_t));
    if (!cache) return NULL;

    // Initialize cache parameters
    cache->cache_s = s;
    cache->cache_E = E;
    cache->cache_delay = delay;
    cache->cache_next = nextlevel;

    int num_sets = 1 << s;
    cache->cache_sets = malloc(num_sets * sizeof(cacheset_t));
    if (!cache->cache_sets) {
        free(cache);
        return NULL;
    }

    // Initialize each set
    for (int i = 0; i < num_sets; i++) {
        cache->cache_sets[i].lines = malloc(E * sizeof(cacheline_t));
        if (!cache->cache_sets[i].lines) {
            // Cleanup memory
            for (int j = 0; j < i; j++) {
                free(cache->cache_sets[j].lines);
                free(((lru_info_t *)cache->cache_sets[j].useinfo)->lru_order);
                free(cache->cache_sets[j].useinfo);
            }
            free(cache->cache_sets);
            free(cache);
            return NULL;
        }

        // Initialize LRU info
        lru_info_t *lru_info = malloc(sizeof(lru_info_t));
        if (!lru_info) {
            // Cleanup if fails
            for (int j = 0; j < i; j++) {
                free(cache->cache_sets[j].lines);
                free(((lru_info_t *)cache->cache_sets[j].useinfo)->lru_order);
                free(cache->cache_sets[j].useinfo);
            }
            free(cache->cache_sets[i].lines);
            free(cache->cache_sets);
            free(cache);
            return NULL;
        }

        lru_info->lru_order = malloc(E * sizeof(int));
        if (!lru_info->lru_order) {
            // Cleanup if fails
            for (int j = 0; j < i; j++) {
                free(cache->cache_sets[j].lines);
                free(((lru_info_t *)cache->cache_sets[j].useinfo)->lru_order);
                free(cache->cache_sets[j].useinfo);
            }
            free(lru_info);
            free(cache->cache_sets[i].lines);
            free(cache->cache_sets);
            free(cache);
            return NULL;
        }

        // Initialize LRU order
        for (int j = 0; j < E; j++) {
            lru_info->lru_order[j] = j;
        }

        cache->cache_sets[i].useinfo = lru_info;

        // Initialize cache lines
        for (int j = 0; j < E; j++) {
            cache->cache_sets[i].lines[j].block = malloc(1 << LOGBSIZE);
            if (!cache->cache_sets[i].lines[j].block) {
                // Cleanup if fails
                for (int k = 0; k < j; k++) {
                    free(cache->cache_sets[i].lines[k].block);
                }
                for (int k = 0; k < i; k++) {
                    for (int l = 0; l < E; l++) {
                        free(cache->cache_sets[k].lines[l].block);
                    }
                    free(cache->cache_sets[k].lines);
                    free(((lru_info_t *)cache->cache_sets[k].useinfo)->lru_order);
                    free(cache->cache_sets[k].useinfo);
                }
                free(lru_info->lru_order);
                free(lru_info);
                free(cache->cache_sets[i].lines);
                free(cache->cache_sets);
                free(cache);
                return NULL;
            }
            cache->cache_sets[i].lines[j].valid = 0;
            cache->cache_sets[i].lines[j].dirty = 0;
        }
    }

    return cache;
}

int cache_access(cache_t *c, addr_t addr, void *value, int size, int iswrite) {
    if (!c) return 0;

    int delay = c->cache_delay;
    int set_index = get_set_index(addr, c->cache_s);
    addr_t tag = get_tag(addr, c->cache_s);
    cacheset_t *set = &c->cache_sets[set_index];
    lru_info_t *lru = (lru_info_t *)set->useinfo;
    
    int offset = addr & ((1 << LOGBSIZE) - 1);
    
    // Check for cache hit
    for (int i = 0; i < c->cache_E; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag) {
            // Cache hit
            update_lru(lru, i, c->cache_E);
            
            if (iswrite) {
                memcpy(set->lines[i].block + offset, value, size);
                set->lines[i].dirty = 1;
            } else {
                memcpy(value, set->lines[i].block + offset, size);
            }
            return delay;  // Return just cache delay for hits
        }
    }

    // Cache miss - find empty line or evict
    int victim_line = -1;
    for (int i = 0; i < c->cache_E; i++) {
        if (!set->lines[i].valid) {
            victim_line = i;
            break;
        }
    }

    // If no empty line, use LRU to select victim
    if (victim_line == -1) {
        victim_line = get_lru_victim(lru);
        
        // If victim is dirty, write it back first
        if (set->lines[victim_line].valid && set->lines[victim_line].dirty) {
            addr_t old_addr = (set->lines[victim_line].tag << (c->cache_s + LOGBSIZE)) |
                             (set_index << LOGBSIZE);
            delay += mem_access(old_addr, set->lines[victim_line].block, 1 << LOGBSIZE, 1);
        }
    }

    // Load new block
    addr_t block_addr = addr & ~((1 << LOGBSIZE) - 1);
    if (c->cache_next) {
        delay += cache_access(c->cache_next, block_addr, 
                            set->lines[victim_line].block, 1 << LOGBSIZE, 0);
    } else {
        delay += mem_access(block_addr, set->lines[victim_line].block, 
                           1 << LOGBSIZE, 0);
    }

    // Update cache line and LRU
    set->lines[victim_line].valid = 1;
    set->lines[victim_line].dirty = iswrite;
    set->lines[victim_line].tag = tag;
    update_lru(lru, victim_line, c->cache_E);

    // Perform requested operation
    if (iswrite) {
        memcpy(set->lines[victim_line].block + offset, value, size);
    } else {
        memcpy(value, set->lines[victim_line].block + offset, size);
    }

    return delay;
}

void cache_flush(cache_t *c) {
    if (!c) return;

    int num_sets = 1 << c->cache_s;
    
    // Flush each set
    for (int i = 0; i < num_sets; i++) {
        cacheset_t *set = &c->cache_sets[i];
        
        // Write back dirty blocks and invalidate lines
        for (int j = 0; j < c->cache_E; j++) {
            if (set->lines[j].valid && set->lines[j].dirty) {
                addr_t addr = (set->lines[j].tag << (c->cache_s + LOGBSIZE)) | (i << LOGBSIZE);
                mem_access(addr, set->lines[j].block, 1 << LOGBSIZE, 1);
            }
            set->lines[j].valid = 0;
            set->lines[j].dirty = 0;
        }

        // Reset LRU order
        lru_info_t *lru = (lru_info_t *)set->useinfo;
        for (int j = 0; j < c->cache_E; j++) {
            lru->lru_order[j] = j;
        }
    }

    // Recursively flush next level
    if (c->cache_next) {
        cache_flush(c->cache_next);
    }
}
