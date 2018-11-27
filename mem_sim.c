/***************************************************************************
 * *    Inf2C-CS Coursework 2: Cache Simulation
 * *
 * *    Instructor: Boris Grot
 * *
 * *    TA: Siavash Katebzadeh
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <time.h>
/* Do not add any more header files */

/*
 * Various structures
 */
typedef enum {FIFO, LRU, Random} replacement_p;

const char* get_replacement_policy(uint32_t p) {
    switch(p) {
    case FIFO: return "FIFO";
    case LRU: return "LRU";
    case Random: return "Random";
    default: assert(0); return "";
    };
    return "";
}

typedef struct {
    uint32_t address;
} mem_access_t;

// These are statistics for the cache and should be maintained by you.
typedef struct {
    uint32_t cache_hits;
    uint32_t cache_misses;
} result_t;


/*
 * Parameters for the cache that will be populated by the provided code skeleton.
 */

replacement_p replacement_policy = FIFO;
uint32_t associativity = 0;
uint32_t number_of_cache_blocks = 0;
uint32_t cache_block_size = 0;


/*
 * Each of the variables below must be populated by you.
 */
uint32_t g_num_cache_tag_bits = 0;
uint32_t g_cache_offset_bits = 0;
result_t g_result;


/* Reads a memory access from the trace file and returns
 * 32-bit physical memory address
 */
mem_access_t read_transaction(FILE *ptr_file) {
    char buf[1002];
    char* token = NULL;
    char* string = buf;
    mem_access_t access;

    if (fgets(buf, 1000, ptr_file)!= NULL) {
        /* Get the address */
        token = strsep(&string, " \n");
        access.address = (uint32_t)strtoul(token, NULL, 16);
        return access;
    }

    /* If there are no more entries in the file return an address 0 */
    access.address = 0;
    return access;
}

void print_statistics(uint32_t num_cache_tag_bits, uint32_t cache_offset_bits, result_t* r) {
    /* Do Not Modify This Function */

    uint32_t cache_total_hits = r->cache_hits;
    uint32_t cache_total_misses = r->cache_misses;
    printf("CacheTagBits:%u\n", num_cache_tag_bits);
    printf("CacheOffsetBits:%u\n", cache_offset_bits);
    printf("Cache:hits:%u\n", r->cache_hits);
    printf("Cache:misses:%u\n", r->cache_misses);
    printf("Cache:hit-rate:%2.1f%%\n", cache_total_hits / (float)(cache_total_hits + cache_total_misses) * 100.0);
}

/*  
 *  Data strucutre models for the building blocks of the cache. These ignore
 *  the data in it, as suggested, as it is beyond the point of the assignment
 */

// models a cache block
typedef struct {
    uint32_t tag;
    uint32_t valid;
    uint32_t last_access; // for LRU
    uint32_t added_on;    // for FIFO
} block_t;

// models a set of cache blocks
typedef struct {
    block_t *blocks;
} set_t;

// encapsulates the pointer to the sets as a cache data structure
/* I am aware this is unnecessary, but I think it helps conceptualise
 *  and read the code, so I believe it is a good idea to keep it. */
typedef struct {
    set_t *sets;
} cache_t;


/* Create a global variable for the cache so it can easily be 
 * accessed in the different fucntions without passing a pointer
 * around. */ 
cache_t my_cache;


uint32_t get_tag(uint32_t address) {
    /* Create a bitmask to isolate the part of the address that corresponds
     * to the tag. we know the number of tag bits, so we can create a 32-bit
     * max value, and then shift left to introduce the 0's */
    uint32_t bitmask = 0xffffffff;
    bitmask <<= 32 - g_num_cache_tag_bits;
    // doing the bitwise AND operation with the address, we get the tag
    uint32_t tag = address & bitmask;
    /* Finally, we shift it right to have it as the smallest number possible,
     * ie, in the least significant bits. This is because in a real cache only
     * the minimum amount of bits is stored to save space */
    tag >>= 32 - g_num_cache_tag_bits;
    return tag;
}

uint32_t get_index(uint32_t address) {
    /* To extract the index bits, a process similar to the tag is used.
     * However, since these bits are in the middle, it requires the bitmask to
     * be shifted twice until the right sequence of bits is reached */
    uint32_t bitmask = 0xffffffff;
    // 1 - Introduce 0's on the left to ignore the tag bits
    bitmask >>= g_num_cache_tag_bits;
    // 2 - Introduce 0's on the right to ignore the offset bits
    bitmask >>= g_cache_offset_bits;
    bitmask <<= g_cache_offset_bits;
    // 3 - Apply the mask
    uint32_t index = address & bitmask;
    // Minimise number of bits being used, as explained in get_tag()
    index >>= g_cache_offset_bits;
    return index;
}


void replace_fifo(uint32_t  tag, set_t curr_set, uint32_t access_number) {
    uint32_t first_in = curr_set.blocks[0].added_on;
    uint32_t first_in_idx = 0;
    // loop through the set, looking for the FIFO block, ie. the "oldest"
    for (int i = 0; i < associativity; i++) {
        block_t *curr_block = &curr_set.blocks[i];
        /* if  the current block was added before the one that is to be
         * evicted, set the curr one to be evicted instead */
        if (curr_block->added_on < first_in) {
            first_in = curr_block->added_on;
            first_in_idx = i;
        }
    }
    // Evict the block that has been in cache for the longest time
    curr_set.blocks[first_in_idx].added_on = access_number;
    curr_set.blocks[first_in_idx].tag = tag;
    /* At this point in execution, all blocks are guaranteed to already
     * have the valid bit already set, so there is no need to do that */
}

void replace_lru(uint32_t  tag, set_t curr_set, uint32_t access_number) {
    uint32_t least_recent_access = curr_set.blocks[0].last_access;
    uint32_t least_recent_access_idx = 0;
    // loop through the set, looking for the LRU block
    for (int i = 0; i < associativity; i++) {
        block_t *curr_block = &curr_set.blocks[i];
        /* if the last access to the current block happened before the oldest
         * known access so far, set the current block to be evicted */
        if (curr_block->last_access < least_recent_access) {
            least_recent_access = curr_block->last_access;
            least_recent_access_idx = i; 
        }  
    }
    // Evict the block that has "been inactive" for the longest time
    curr_set.blocks[least_recent_access_idx].last_access = access_number;
    curr_set.blocks[least_recent_access_idx].tag = tag;
    // Again, valid bit should already be set
}

void replace_random(uint32_t  tag, set_t curr_set) {
    // Evict a random block within the set
    uint32_t r = rand() % associativity;
    curr_set.blocks[r].tag = tag;
    // curr_set.blocks[r].valid = 1;
    return;
}


void write_to_cache(uint32_t tag, set_t curr_set, uint32_t access_number) {
    // Look for an invalid or empty block. If there is one, use it!
    for (int i = 0; i < associativity; i ++) {
        block_t *curr_block = &curr_set.blocks[i];
        if (!curr_block->valid) {
            curr_block->tag = tag;
            curr_block->valid = 1;
            curr_block->added_on = access_number;
            curr_block->last_access = access_number;
            return;
        }
    }
    // If there is no available block, replace one, according to the policy
    switch (replacement_policy) {
        case FIFO:
            replace_fifo(tag, curr_set, access_number);
            break;
        case LRU:
            replace_lru(tag, curr_set, access_number);
            break;
        case Random:
            replace_random(tag, curr_set);
            break;
    }
    return;
}


void access_cache(uint32_t address, uint32_t access_number) {
    
    uint32_t tag = get_tag(address);
    uint32_t index = get_index(address);

    /* For readability we introduce a current set. That is, the set
     * the given address is conditioned to correspond to, given the
     * associativity. Because the set struct points to the blocks, 
     * rather than sotring them locally, we don't need to make this
     * current set a pointer. */
    set_t curr_set = my_cache.sets[index];

    for (int i = 0; i < associativity; i++) {
        /* Again, for readability, we introduce a current block. However,
         * this time it must be a pointer to block in the set, otherwise
         * the changes would have no effect. */
        block_t *curr_block = &curr_set.blocks[i];
        /* On a hit: update last_access field (whether or not LRU is the
         * replacement policy, since we would have to check ) */
        if (curr_block->tag == tag && curr_block->valid) {
            curr_block->last_access = access_number;
            g_result.cache_hits++; // HIT!
            return;
        }
    }

    // If execution reaches this point:
    g_result.cache_misses++; // MISS!

    // Finally, we write the requested block to cache
    write_to_cache(tag, curr_set, access_number);

    return;
}


int main(int argc, char** argv) {
    time_t t;
    /* Intializes random number generator */
    /* Important: *DO NOT* call this function anywhere else. */
    srand((unsigned) time(&t));
    /* ----------------------------------------------------- */
    /* ----------------------------------------------------- */

    /*
     *
     * Read command-line parameters and initialize configuration variables.
     *
     */
    int improper_args = 0;
    char file[10000];
    if (argc < 6) {
        improper_args = 1;
        printf("Usage: ./mem_sim [replacement_policy: FIFO LRU Random] [associativity: 1 2 4 8 ...] [number_of_cache_blocks: 16 64 256 1024] [cache_block_size: 32 64] mem_trace.txt\n");
    } else  {
        /* argv[0] is program name, parameters start with argv[1] */
        if (strcmp(argv[1], "FIFO") == 0) {
            replacement_policy = FIFO;
        } else if (strcmp(argv[1], "LRU") == 0) {
            replacement_policy = LRU;
        } else if (strcmp(argv[1], "Random") == 0) {
            replacement_policy = Random;
        } else {
            improper_args = 1;
            printf("Usage: ./mem_sim [replacement_policy: FIFO LRU Random] [associativity: 1 2 4 8 ...] [number_of_cache_blocks: 16 64 256 1024] [cache_block_size: 32 64] mem_trace.txt\n");
        }
        associativity = atoi(argv[2]);
        number_of_cache_blocks = atoi(argv[3]);
        cache_block_size = atoi(argv[4]);
        strcpy(file, argv[5]);
    }
    if (improper_args) {
        exit(-1);
    }
    assert(number_of_cache_blocks == 16 || number_of_cache_blocks == 64 || number_of_cache_blocks == 256 || number_of_cache_blocks == 1024);
    assert(cache_block_size == 32 || cache_block_size == 64);
    assert(number_of_cache_blocks >= associativity);
    assert(associativity >= 1);

    printf("input:trace_file: %s\n", file);
    printf("input:replacement_policy: %s\n", get_replacement_policy(replacement_policy));
    printf("input:associativity: %u\n", associativity);
    printf("input:number_of_cache_blocks: %u\n", number_of_cache_blocks);
    printf("input:cache_block_size: %u\n", cache_block_size);
    printf("\n");

    /* Open the file mem_trace.txt to read memory accesses */
    FILE *ptr_file;
    ptr_file = fopen(file,"r");
    if (!ptr_file) {
        printf("Unable to open the trace file: %s\n", file);
        exit(-1);
    }

    /* result structure is initialized for you. */
    memset(&g_result, 0, sizeof(result_t));

    /* Do not delete any of the lines below.
     * Use the following snippet and add your code to finish the task. */

    /* You may want to setup your Cache structure here. */
    
    // TODO: check if all these variables need to be global!!!
    // Calculate appropriate number of bits for requested configuration
    g_cache_offset_bits = log2(cache_block_size);
    uint32_t num_cache_sets = number_of_cache_blocks / associativity;
    uint32_t cache_index_bits = log2(num_cache_sets);
    g_num_cache_tag_bits = 32 - cache_index_bits - g_cache_offset_bits;

    my_cache.sets = malloc(num_cache_sets * sizeof(set_t));
    
    for (int i = 0; i < num_cache_sets; i++) {
        my_cache.sets[i].blocks = malloc(associativity * sizeof(block_t));
        for (int j = 0; j < associativity; j++) {
            my_cache.sets[i].blocks[j].valid = 0;
            my_cache.sets[i].blocks[j].tag = -1;
            my_cache.sets[i].blocks[j].added_on = UINT32_MAX;
            my_cache.sets[i].blocks[j].last_access = UINT32_MAX;
        }
    }

    uint32_t access_number = 0;

    mem_access_t access;
    /* Loop until the whole trace file has been read. */
    while(1) {
        access = read_transaction(ptr_file);
        // If no transactions left, break out of loop.
        if (access.address == 0)
            break;

        /* Add your code here */
        access_cache(access.address, access_number);

        access_number++;

    }
    
    // free the allocated memory: first the pointer to each set of blocks
    for (int i = 0; i < num_cache_sets; i++) {
        free(my_cache.sets[i].blocks);
    }
    // then the pointer to the sets
    free(my_cache.sets);

    /* Do not modify code below. */
    /* Make sure that all the parameters are appropriately populated. */
    print_statistics(g_num_cache_tag_bits, g_cache_offset_bits, &g_result);

    /* Close the trace file. */
    fclose(ptr_file);
    return 0;
}
