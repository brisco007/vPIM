/*
* BFS with multiple tasklets
*
*/
#include <stdio.h>
#include <alloc.h>
#include <barrier.h>
#include <defs.h>
#include <mram.h>
#include <mutex.h>
#include <perfcounter.h>
#include "../support/common.h"

__host struct Parameters PARAM;
__host uint64_t* FRONTIER_VISITED;
uint32_t verbose = 1;

BARRIER_INIT(bfsBarrier, NR_TASKLETS);
BARRIER_INIT(endBarrier, NR_TASKLETS);
MUTEX_INIT(nextFrontierMutex);

void mram_mem_copy(uint32_t src, uint32_t dst, uint32_t size) {
    #define BUFFER_SIZE 32
    uint8_t buffer[BUFFER_SIZE];
    size = ROUND_UP_TO_MULTIPLE_OF_8(size);
    while (size > 0) {
        uint32_t bytes_to_process = size < BUFFER_SIZE ? size : BUFFER_SIZE;
        mram_read((__mram_ptr void const*) src, buffer, bytes_to_process);
        mram_write(buffer, (__mram_ptr void*) dst, bytes_to_process);
        src += bytes_to_process;
        dst += bytes_to_process;
        size -= bytes_to_process;
    }

    #undef BUFFER_SIZE
}

int getBitAtBatch(uint32_t address, uint32_t i) {
    uint32_t arrayIndex = i / 32;          // Determine which uint32_t contains the bit for index i
    uint32_t bitPosition = i % 32;         // Determine the bit position within that uint32_t

    // Calculate the MRAM address to read from
    uint32_t mram_address_to_read = address + arrayIndex * sizeof(uint32_t);

    // Read the required 8 bytes from MRAM into a buffer
    uint64_t buffer;
    mram_read((__mram_ptr void const*)mram_address_to_read, &buffer, sizeof(buffer));

    // Extract the required uint32_t value from the buffer
    uint32_t value = (arrayIndex % 2 == 0) ? (uint32_t)buffer : (uint32_t)(buffer >> 32);

    // Create a mask to isolate the desired bit
    uint32_t mask = (uint32_t)1 << bitPosition;

    // Check if the bit is set and return 1 or 0
    return (value & mask) ? 1 : 0;
}

void setBitAtBatch(uint32_t address, uint32_t i, uint32_t b) {
    uint32_t arrayIndex = i / 32;          // Determine which uint32_t contains the bit for index i
    uint32_t bitPosition = i % 32;         // Determine the bit position within that uint32_t

    // Calculate the MRAM address to read from
    uint32_t mram_address_to_read = address + arrayIndex * sizeof(uint32_t);

    // Read the required 8 bytes from MRAM into a buffer
    uint64_t buffer;
    mram_read((__mram_ptr void const*)mram_address_to_read, &buffer, sizeof(buffer));

    // Extract the required uint32_t value from the buffer
    uint32_t value = (arrayIndex % 2 == 0) ? (uint32_t)buffer : (uint32_t)(buffer >> 32);

    // Create a mask to isolate the desired bit
    uint32_t mask = (uint32_t)1 << bitPosition;

    if (b) {
        // Set the bit to 1
        value |= mask;
    } else {
        // Set the bit to 0
        mask = ~mask;
        value &= mask;
    }

    // Update the buffer with the modified value
    if (arrayIndex % 2 == 0) {
        buffer = (buffer & 0xFFFFFFFF00000000ULL) | (uint64_t)value;
    } else {
        buffer = (buffer & 0x00000000FFFFFFFFULL) | ((uint64_t)value << 32);
    }

    // Write the modified 8 bytes back to MRAM
    mram_write(&buffer, (__mram_ptr void*)mram_address_to_read, sizeof(buffer));
}

void resetBitVectorInMRAM(uint32_t address, uint32_t bitvector_length) {
    uint32_t num_uint32s = (bitvector_length + 31) / 32;  // Calculate the number of uint32_t values in the bitvector

    // Create a buffer filled with zeros
    uint64_t buffer = 0;

    // Write zeros for all complete 8-byte chunks
    for (uint32_t i = 0; i < (num_uint32s / 2); i++) {
        mram_write(&buffer, (__mram_ptr void*)(address + i * sizeof(uint64_t)), sizeof(buffer));
    }

    // If there's an odd number of uint32_t values, handle the last chunk
    if (num_uint32s % 2 != 0) {
        uint64_t last_chunk;
        uint32_t mram_address_to_read = address + (num_uint32s - 1) * sizeof(uint32_t);
        
        // Read the last 8-byte chunk
        mram_read((__mram_ptr void const*)mram_address_to_read, &last_chunk, sizeof(last_chunk));

        // Set the first uint32_t of the chunk to zero
        last_chunk &= 0xFFFFFFFF00000000ULL;

        // Write the modified chunk back
        mram_write(&last_chunk, (__mram_ptr void*)mram_address_to_read, sizeof(last_chunk));
    }
}


uint32_t getNodeOffsetInPartitionArray(uint32_t mram_base_addr, uint32_t index){
    uint32_t current_offset = 0; 
    // Iterate over each node until we reach the desired index
    for (uint32_t i = 0; i < index; i++) {
        uint32_t data[2]; // Storage for index and size
        mram_read((__mram_ptr void const*)(mram_base_addr + current_offset), &data, sizeof(data));
        uint32_t node_size = data[1];
        current_offset += sizeof(data) + node_size * sizeof(uint32_t);
    }
    return current_offset;
}

void getNodeInfoInPartitionArray(uint32_t mram_base_addr, uint32_t offset, uint32_t *index, uint32_t *size){
    uint32_t data[2]; 
    mram_read((__mram_ptr void const*)(mram_base_addr + offset), &data, sizeof(data));
    *index = data[0];
    *size = data[1];
}


uint32_t getNextBatch(uint32_t mram_base_addr, uint32_t *offset, uint32_t batchsize, uint32_t *batch, uint32_t max_length) {
    for (uint32_t i = 0; i < max_length; i++) {
        batch[i] = 0;
    }
    // Calculate the number of full 8-byte chunks to read
    uint32_t fullChunks = batchsize / 2;

    for (uint32_t i = 0; i < fullChunks; i++) {
        mram_read((__mram_ptr void const*)(mram_base_addr + *offset), &batch[i * 2], 8);
        *offset += 8;
    }
    // Handle the case where batchsize is odd and there's one more uint32_t to read
    if (batchsize % 2) {
        uint32_t lastData[2];
        mram_read((__mram_ptr void const*)(mram_base_addr + *offset), &lastData, 8);
        batch[batchsize - 1] = lastData[0];
        *offset += 4;  // Only increase the offset by 4 bytes since we read one uint32_t
    }
    return batchsize;  // Return the number of elements copied to the batch
}

void printBitVectorIndexInMRAM(uint32_t address, uint32_t bitvector_length) {
    uint32_t num_uint32s = (bitvector_length + 31) / 32;  // Calculate the number of uint32_t values in the bitvector

    for (uint32_t i = 0; i < num_uint32s; i += 2) {
        // Read 8 bytes from MRAM
        uint32_t buffer[2];
        mram_read((__mram_ptr void const*)(address + i * sizeof(uint32_t)), &buffer, sizeof(buffer));

        // Process each uint32_t separately
        for (int k = 0; k < 2 && (i + k) < num_uint32s; k++) {
            for (int j = 0; j < 32 && (i * 32 + k * 32 + j) < bitvector_length; j++) {
                if (buffer[k] & (1U << j)) {
                    printf("%u ", i * 32 + k * 32 + j);
                }
            }
        }
    }
    printf("\n");  // Newline after printing all indices
}


int main() {
    //parameters
    struct Parameters param = PARAM;
    uint32_t partition_size = param.partition_size;
    uint32_t nr_total_nodes = param.nr_total_nodes;
    uint32_t nr_nodes_partition = (uint32_t) param.nr_nodes_partition;
    uint32_t vectorSize = ROUND_UP_TO_MULTIPLE_OF_2((nr_total_nodes + 31) / 32);
    //heap pointers
    uint32_t mram_base_addr = (uint32_t)DPU_MRAM_HEAP_POINTER;
    uint32_t mram_frontier_addr = (uint32_t) (DPU_MRAM_HEAP_POINTER + partition_size);
    uint32_t mram_visited_addr = (uint32_t) (DPU_MRAM_HEAP_POINTER + partition_size + vectorSize * sizeof(uint32_t));
    uint32_t mram_next_frontier_addr = (uint32_t) (DPU_MRAM_HEAP_POINTER + partition_size + vectorSize * 2 * sizeof(uint32_t));
    if(me()==0) {
        mem_reset(); // Reset the heap
        resetBitVectorInMRAM(mram_next_frontier_addr, nr_total_nodes);
    }
    barrier_wait(&bfsBarrier);
    // Identify tasklet's nodes
    uint32_t numNodesPerTasklet = (nr_nodes_partition + NR_TASKLETS - 1)/NR_TASKLETS;
    uint32_t taskletNodesStart = me()*numNodesPerTasklet;
    uint32_t taskletNumNodes;
    if(taskletNodesStart > nr_nodes_partition) {
        taskletNumNodes = 0;
    } else if(taskletNodesStart + numNodesPerTasklet > nr_nodes_partition) {
        taskletNumNodes = nr_nodes_partition - taskletNodesStart;
    } else {
        taskletNumNodes = numNodesPerTasklet;
    }
    //BFS Algorithm
    uint32_t batch_size = 2;
    uint32_t *batch = (uint32_t *) mem_alloc(batch_size * sizeof(uint32_t));
    mutex_id_t mutexID = MUTEX_GET(nextFrontierMutex);
    uint32_t curNodeOffset;
    for(uint32_t i=taskletNodesStart; i<taskletNodesStart + taskletNumNodes; i++){
        if(i==taskletNodesStart) {
            if(param.offsets[me()]==0) {
                curNodeOffset = getNodeOffsetInPartitionArray(mram_base_addr, i);
                param.offsets[me()] = curNodeOffset;
            }
            else{
                curNodeOffset = param.offsets[me()];
            }
        }
        uint32_t curNodeIndex;
        uint32_t curNodeSize;
        getNodeInfoInPartitionArray(mram_base_addr, curNodeOffset, &curNodeIndex, &curNodeSize);
        curNodeOffset += 2 * sizeof(uint32_t);
        if(getBitAtBatch(mram_frontier_addr, curNodeIndex)){
            uint32_t processed_neighbors = 0;
            while(processed_neighbors < curNodeSize){
                uint32_t curBatch = batch_size > curNodeSize-processed_neighbors? curNodeSize-processed_neighbors:batch_size;
                getNextBatch(mram_base_addr, &curNodeOffset, curBatch, batch, batch_size);
                for(uint32_t j=0; j<curBatch; j++){
                    if(getBitAtBatch(mram_visited_addr, batch[j])==0){
                        mutex_lock(mutexID);
                        setBitAtBatch(mram_next_frontier_addr, batch[j], 1);
                        setBitAtBatch(mram_visited_addr, batch[j], 1);
                        mutex_unlock(mutexID); 
                    }
                }
                processed_neighbors += curBatch;
            }
        }
        else curNodeOffset+= curNodeSize * sizeof(uint32_t);
    }
    barrier_wait(&endBarrier);
    if(me()==0) mram_mem_copy(mram_next_frontier_addr, mram_frontier_addr, vectorSize * sizeof(uint32_t));
    return 0;
}