#ifndef _COMMON_H_
#define _COMMON_H_

#define ROUND_UP_TO_MULTIPLE_OF_2(x)    ((((x) + 1)/2)*2)
#define ROUND_UP_TO_MULTIPLE_OF_8(x)    ((((x) + 7)/8)*8)
#define ROUND_UP_TO_MULTIPLE_OF_64(x)   ((((x) + 63)/64)*64)

#define setBit(val, idx) (val) |= (1 << (idx))
#define isSet(val, idx)  ((val) & (1 << (idx)))

struct Parameters{
    uint32_t partition_size;
    uint32_t nr_total_nodes;
    uint64_t nr_nodes_partition;
    uint32_t offsets[16];
};

struct Node{
    uint32_t index;
    uint32_t size;
    uint32_t* neighbors;
};

struct Graph{
    uint32_t numNodes;
    struct Node* nodes;
};

uint32_t getNodeSize(struct Node node) {
    return (2 + node.size) * sizeof(uint32_t);
}

uint32_t getSizeLargestNode(struct Graph graph) {
    uint32_t maxSize = 0;
    for (uint32_t i = 0; i < graph.numNodes; i++) {
        uint32_t currentSize = getNodeSize(graph.nodes[i]);
        if (currentSize > maxSize) {
            maxSize = currentSize;
        }
    }
    return maxSize;
}

int getBitAt(uint32_t* vector, uint32_t i) {
    uint32_t arrayIndex = i / 32;          // Determine which uint32_t contains the bit for index i
    uint32_t bitPosition = i % 32;         // Determine the bit position within that uint32_t
    uint32_t mask = (uint32_t)1 << bitPosition;  // Create a mask to isolate the desired bit
    return (vector[arrayIndex] & mask) ? 1 : 0;  // Check if the bit is set and return 1 or 0
}

void setBitAt(uint32_t* vector, uint32_t i, uint32_t b) {
    uint32_t arrayIndex = i / 32;          // Determine which uint32_t contains the bit for index i
    uint32_t bitPosition = i % 32;         // Determine the bit position within that uint32_t
    uint32_t mask = (uint32_t)1 << bitPosition;  // Create a mask to isolate the desired bit
    if (b == 1) {
        vector[arrayIndex] |= mask;        // Set the bit
    } else {
        vector[arrayIndex] &= ~mask;       // Clear the bit
    }
}

int compareBitAt(uint32_t* vector1, uint32_t* vector2, uint32_t index) {
    uint32_t arrayIndex = index / 32;          // Determine which uint32_t contains the bit for the given index
    uint32_t bitPosition = index % 32;         // Determine the bit position within that uint32_t
    uint32_t mask = (uint32_t)1 << bitPosition;  // Create a mask to isolate the desired bit
    // Check if the bit at the specified index is the same in both vectors
    // The function will return 1 (true) if the bits are the same and 0 (false) otherwise.
    return (vector1[arrayIndex] & mask) == (vector2[arrayIndex] & mask);
}

int peekFrontier(uint32_t* frontier, uint32_t numNodes) {
    for (uint32_t i = 0; i < numNodes; i++) {
        if (getBitAt(frontier, i) == 1) {
            return i;  // Return the index of the first nonzero bit
        }
    }
    return -1;  // Return -1 if no nonzero bit is found
}

uint32_t popFrontier(uint32_t* frontier, uint32_t numNodes) {
    for (uint32_t i = 0; i < numNodes; i++) {
        if (getBitAt(frontier, i) == 1) {
            setBitAt(frontier, i, 0);  // Set the bit to 0
            return i;  // Return the index of the bit that was set to 0
        }
    }
    return -1;  // Return -1 if no nonzero bit is found
}


void printVector(uint32_t* vector, uint32_t numElements) {
    for (uint32_t i = 0; i < numElements; i++) {
        uint32_t arrayIndex = i / 32;          // Determine which uint32_t contains the bit for index i
        uint32_t bitPosition = i % 32;         // Determine the bit position within that uint32_t
        uint32_t mask = (uint32_t)1 << bitPosition;  // Create a mask to isolate the desired bit
        // Print the bit value (either 0 or 1)
        printf("%d", (vector[arrayIndex] & mask) ? 1 : 0);
        // Optionally, you can add a space or newline after every 32 bits for better readability
        if (bitPosition == 31) {
            printf("\n");
        }
    }
    printf("\n");  // Print a newline at the end for clarity
}

void printVectorIndex(uint32_t* vector, uint32_t numElements) {
    for (uint32_t i = 0; i < numElements; i++) {
        uint32_t arrayIndex = i / 32;          // Determine which uint32_t contains the bit for index i
        uint32_t bitPosition = i % 32;         // Determine the bit position within that uint32_t
        uint32_t mask = (uint32_t)1 << bitPosition;  // Create a mask to isolate the desired bit

        // Check if the bit is set and print its index
        if (vector[arrayIndex] & mask) {
            printf("%u ", i);
        }
    }
    printf("\n\n");
}

uint32_t printFrontierSize(uint32_t* vector, uint32_t numElements) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < numElements; i++) {
        uint32_t arrayIndex = i / 32;          // Determine which uint32_t contains the bit for index i
        uint32_t bitPosition = i % 32;         // Determine the bit position within that uint32_t
        uint32_t mask = (uint32_t)1 << bitPosition;  // Create a mask to isolate the desired bit

        // Check if the bit is set and print its index
        if (vector[arrayIndex] & mask) {
            count++;
        }
    }
    return count;
}

void printGraph(struct Graph g) {
    printf("Whole Graph with %u nodes:\n", g.numNodes);
    printf("-----------------\n");
    for (uint32_t i = 0; i < g.numNodes; i++) {
        printf("Node %u: ", g.nodes[i].index);
        for (uint32_t j = 0; j < g.nodes[i].size; j++) {
            printf("%u ", g.nodes[i].neighbors[j]);
        }
        printf("\n");
    }
    
    printf("\n");
}

void printGraphWithLevel(struct Graph graph, int* levels) {
    printf("Graph:\n");
    for (uint32_t i = 0; i < graph.numNodes; i++) {
        printf("Node Index: %u, Level: %d, Neighbors: ", graph.nodes[i].index, levels[i]);
        for (uint32_t j = 0; j < graph.nodes[i].size; j++) {
            printf("%u ", graph.nodes[i].neighbors[j]);
        }
        printf("\n");
    }
    printf("\n");
}


#endif

