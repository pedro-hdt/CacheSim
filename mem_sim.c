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
        access.address = (uint32_t)strtol(token, NULL, 16);
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
 *
 * Add any global variables and/or functions here as needed.
 *
 */



/*  
 *  data-type models for the constituents of the cache 
 *  these ignore the data part, as suggested, as it is
 *  beyond the point of the assignment
 */

typedef struct {
    uint32_t tag;
    uint32_t valid;
    uint32_t LRU;
    uint32_t FIFO;
} block_t;


typedef struct {
    uint32_t index;
    block_t *blocks;
} set_t;


typedef struct {
    set_t *sets;
} cache_t;


uint32_t g_cache_index_bits = 0;
uint32_t g_num_cache_sets = 0;
cache_t my_cache;


int is_full(set_t set) {
    for (int i = 0; i < associativity; i++) {
        block_t curr_block = set.blocks[i];
        if (curr_block.valid == 0)
            return 0;
    }
    return 1;
}


uint32_t get_tag(uint32_t address) {
    // create a bitmask to isolate the part of the address that corresponds to the tag
    // we know the number of tag bits, so we can create a 32-bit max value
    // and then shift left by the right amount
    uint32_t bitmask = 0xffffffff << (32 - g_num_cache_tag_bits);
    // doing the bitwise AND operation, we get the tag
    uint32_t tag = address & bitmask;
    return tag;
}

uint32_t get_index(uint32_t address) {
    uint32_t bitmask = 0xffffffff;
    bitmask >>= g_num_cache_tag_bits;
    bitmask <<= g_cache_offset_bits;
    uint32_t index = address & bitmask;
    return index;
}


void access_cache(uint32_t address, uint32_t access_number) {
    
    uint32_t tag = get_tag(address);
    uint32_t index = get_index(address);

    set_t curr_set = my_cache.sets[index];

    for (int i = 0; i < associativity; i++) {
        block_t curr_block = curr_set.blocks[i];
        if (curr_block.tag == tag && curr_block.valid) {
            g_result.cache_hits++;
            curr_block.LRU = access_number;
            return;
        }
    }

    g_result.cache_misses++;

    if (replacement_policy == FIFO) {
        /* keep track of which block to evict 
        in case no empty space is found */
        uint32_t evicted = curr_set.blocks[0].FIFO;
        uint32_t evicted_idx = 0;
        /* loop through the set, looking for invalid data
        that can be overwritten , while at the same time
        looking for the FIFO block in case it is necessary */
        for (int i = 0; i < associativity; i++) {
            block_t curr_block = curr_set.blocks[i];
            /* if curr_block was added before the one that
            is to be evicted, set the curr one to be evicted
            instead */
            if (curr_block.FIFO < evicted) {
                evicted = curr_block.FIFO;
                evicted_idx = i;
            }
            // if there is free space, use it and finish this access
            if (curr_block.valid == 0) {
                curr_block.tag = tag;
                curr_block.valid = 1;
                curr_block.FIFO = access_number;
                return;
            }
        }
        /* if execution reaches this point, there is no free space
        so we evict the block chosen above */
        // since it should already be valid, no need to set valid bit
        curr_set.blocks[evicted_idx].tag = tag;
    }
    else {
        for (int i = 0; i < associativity; i ++) {
            if (is_full(curr_set)) {
                uint32_t r = rand() % number_of_cache_blocks;
                curr_set.blocks[r].tag = tag;
                curr_set.blocks[r].valid = 1;
            }
        }
        
    }

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
    g_num_cache_sets = number_of_cache_blocks / associativity;
    g_cache_index_bits = log2(g_num_cache_sets);
    g_num_cache_tag_bits = 32 - g_cache_index_bits - g_cache_offset_bits;

    my_cache.sets = malloc(g_num_cache_sets * sizeof(set_t));
    
    for (int i = 0; i < sizeof(my_cache.sets); i++) {
        set_t curr_set = my_cache.sets[i];
        curr_set.index = i;
        curr_set.blocks = malloc(associativity * sizeof(block_t));
        for (int j = 0; j < sizeof(curr_set.blocks); j++) {
            block_t curr_block = curr_set.blocks[j];
            curr_block.valid = 0;
            curr_block.tag = -1;
            curr_block.FIFO = -1;
            curr_block.LRU = -1;
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

    /* Do not modify code below. */
    /* Make sure that all the parameters are appropriately populated. */
    print_statistics(g_num_cache_tag_bits, g_cache_offset_bits, &g_result);

    /* Close the trace file. */
    fclose(ptr_file);
    return 0;
}
