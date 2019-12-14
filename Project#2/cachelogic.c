#include "tips.h"

void handlePolicy(unsigned int* lruIndex, unsigned int* lruValue);

/* The following two functions are defined in util.c */

/* finds the highest 1 bit, and returns its position, else 0xFFFFFFFF */
unsigned int uint_log2(word w); 

/* return random int from 0..x-1 */
int randomint( int x );

/*
  This function allows the lfu information to be displayed

    assoc_index - the cache unit that contains the block to be modified
    block_index - the index of the block to be modified

  returns a string representation of the lfu information
 */
char* lfu_to_string(int assoc_index, int block_index)
{
  /* Buffer to print lfu information -- increase size as needed. */
  static char buffer[9];
  sprintf(buffer, "%u", cache[assoc_index].block[block_index].accessCount);

  return buffer;
}

/*
  This function allows the lru information to be displayed

    assoc_index - the cache unit that contains the block to be modified
    block_index - the index of the block to be modified

  returns a string representation of the lru information
 */
char* lru_to_string(int assoc_index, int block_index)
{
  /* Buffer to print lru information -- increase size as needed. */
  static char buffer[9];
  sprintf(buffer, "%u", cache[assoc_index].block[block_index].lru.value);

  return buffer;
}

/*
  This function initializes the lfu information

    assoc_index - the cache unit that contains the block to be modified
    block_number - the index of the block to be modified

*/
void init_lfu(int assoc_index, int block_index)
{
  cache[assoc_index].block[block_index].accessCount = 0;
}

/*
  This function initializes the lru information

    assoc_index - the cache unit that contains the block to be modified
    block_number - the index of the block to be modified

*/
void init_lru(int assoc_index, int block_index)
{
  cache[assoc_index].block[block_index].lru.value = 0;
}

/*
  This is the primary function you are filling out,
  You are free to add helper functions if you need them

  @param addr 32-bit byte address
  @param data a pointer to a SINGLE word (32-bits of data)
  @param we   if we == READ, then data used to return
              information back to CPU

              if we == WRITE, then data used to
              update Cache/DRAM
*/
void accessMemory(address addr, word* data, WriteEnable we)
{
  /* Declare variables here */
  unsigned int indexBits, offsetBits;
  unsigned int indexValue, offsetValue, tagValue;
  unsigned int blockHit, blockIndex;
  TransferUnit transferSize;

  /* handle the case of no cache at all - leave this in */
  if(assoc == 0) {
    accessDRAM(addr, (byte*)data, WORD_SIZE, we);
    return;
  }

  // Calculate number of bits
  offsetBits = uint_log2(block_size);
  indexBits = uint_log2(set_count);
  // unsigned int tagBits = 32 - indexBits - offsetBits;

  // Calculate value of bits
  offsetValue = addr & ((1 << offsetBits) - 1); // Determines offset within a block
  indexValue = (addr >> offsetBits) & ((1 << indexBits) - 1); // Determines which cache set
  tagValue = addr >> (indexBits + offsetBits); // Determines if there is a block match

  // Determine amount of bytes to transfer
  switch (block_size) {
    case 4:
      transferSize = WORD_SIZE;
      break;
    case 8:
      transferSize = DOUBLEWORD_SIZE;
      break;
    case 16:
      transferSize = QUADWORD_SIZE;
      break;
    case 32:
      transferSize = OCTWORD_SIZE;
      break;
  }

  blockHit = 0; // Defaultly assume miss
  // Determine if hit
  for (int i = 0; i < assoc; i++) {
    // Hit
    if (tagValue == cache[indexValue].block[i].tag && cache[indexValue].block[i].valid == VALID) {
      blockHit = 1; // Set as hit
      blockIndex = i; // Save block index
      highlight_offset(indexValue, blockIndex, offsetValue, HIT); // Highlight hit
      break;
    }
  }

  // Miss
  if (blockHit == 0) {
    // Highlight Miss
    highlight_offset(indexValue, blockIndex, offsetValue, MISS);
    highlight_block(indexValue, blockIndex);

    // Get block to replace based on policy
    if (policy == RANDOM) {
      // Get random block
      blockIndex = randomint(assoc);
    } else if (policy == LRU) {
      // get LRU block
      for (int i = 0; i < assoc; i++) {
        if (cache[indexValue].block[i].lru.value == 0) {
          blockIndex = cache[indexValue].block[i].lru.value;
          break;
        }
      }
    }

    // Write-back to DRAM
    if (memory_sync_policy == WRITE_BACK && cache[indexValue].block[blockIndex].dirty == DIRTY) {
      accessDRAM(addr, cache[indexValue].block[blockIndex].data, transferSize, WRITE);
    }

    // Read from DRAM to replace block
    accessDRAM(addr, cache[indexValue].block[blockIndex].data, transferSize, READ);
    cache[indexValue].block[blockIndex].tag = tagValue;
    cache[indexValue].block[blockIndex].valid = VALID;
    if (memory_sync_policy == WRITE_BACK) {
      cache[indexValue].block[blockIndex].dirty = VIRGIN;
    }
  }

  if (we == READ) {
    // Read from cache
    memcpy(data, cache[indexValue].block[blockIndex].data + offsetValue, transferSize);
  } else if (we == WRITE) {
    // Write to cache
    memcpy(cache[indexValue].block[blockIndex].data + offsetValue, data, transferSize);

    // For future write-back to DRAM
    if (memory_sync_policy == WRITE_BACK) {
      cache[indexValue].block[blockIndex].dirty = DIRTY;
    } else if (memory_sync_policy == WRITE_THROUGH) { // Write-through to DRAM
      accessDRAM(addr, cache[indexValue].block[blockIndex].data, transferSize, WRITE);
    }
  }

  // Update LRU values, assoc - 1 means most recently used down to 0 which means least recently used (LRU); acts like a Jenga stack where you can pull from anywhere and put on top
  if (policy == LRU) {
    // Save LRU value that will be set to most recent
    unsigned int oldLRU = cache[indexValue].block[blockIndex].lru.value;
    // Decrement the LRU of all blocks above the oldLRU; this will lead to an order of LRUs from 0 to assoc - 1 if all blocks are used
    for (int i = 0; i < assoc; i++) {
      if (cache[indexValue].block[i].lru.value > oldLRU) { 
        cache[indexValue].block[i].lru.value--;
      }
    }
    // Set LRU value to highest value/most recent
    cache[indexValue].block[blockIndex].lru.value = assoc - 1;
  }
}