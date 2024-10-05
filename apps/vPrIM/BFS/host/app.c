/**
* app.c
* BFS Host Application Source File
*
*/
#include <dpu.h>
#include <dpu_log.h>
#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../support/common.h"
#include "../support/BFS_graph.h"
#include "../support/params.h"
#include "../support/timer.h"
#include "../support/utils.h"
#define DPU_BINARY "./bin/dpu_code"
uint32_t verbose_level = 0;

int* BFS_CPU(struct Graph graph, int source) {
    int numNodes = graph.numNodes;
    int* nodeLevel = malloc(numNodes * sizeof(uint32_t));
    for (int i = 0; i < numNodes; i++) {
        nodeLevel[i] = -1;  // Initialize all nodes as not reachable as 0
    }
    bool* visited = (bool*)calloc(numNodes, sizeof(bool));
    // Create an empty queue for the current frontier and the next frontier
    int* frontier = (int*)malloc(numNodes * sizeof(int));
    int* nextFrontier = (int*)malloc(numNodes * sizeof(int));
    int frontierSize = 0;
    int nextFrontierSize = 0;

    // Start from the source node
    visited[source] = true;
    nodeLevel[source] = 0;
    frontier[frontierSize++] = source;
    // BFS traversal
    while (frontierSize > 0) {
        for (int32_t i = 0; i < frontierSize; i++) {
            int u = frontier[i];
            for (uint32_t j = 0; j < graph.nodes[u].size; j++) {
                int v = graph.nodes[u].neighbors[j];
                if (!visited[v]) {
                    visited[v] = true;
                    nodeLevel[v] = nodeLevel[u] + 1;
                    nextFrontier[nextFrontierSize++] = v;
                }
            }
        }
        // Move to the next frontier
        int* temp = frontier;
        frontier = nextFrontier;
        nextFrontier = temp;
        frontierSize = nextFrontierSize;
        nextFrontierSize = 0;
    }
    free(visited);
    free(frontier);
    free(nextFrontier);
    return nodeLevel;
}

uint32_t* bitVectorUnion(uint32_t** bitVectors, uint32_t numVectors, uint32_t vectorSize) {
    // Allocate memory for the resulting union bit vector
    uint32_t* unionVector = (uint32_t*) calloc(vectorSize, sizeof(uint32_t));
    // Iterate over each bit vector and perform the union operation
    for (uint32_t i = 0; i < numVectors; i++) {
        for (uint32_t j = 0; j < vectorSize; j++) {
            unionVector[j] |= bitVectors[i][j];
        }
    }

    return unionVector;
}

void preprocess_partition(struct Graph partition){
    for (uint32_t i = 0; i < partition.numNodes; i++) {
        if(partition.nodes[i].size%2==1){
            partition.nodes[i].size++;
            partition.nodes[i].neighbors = realloc(partition.nodes[i].neighbors, partition.nodes[i].size*sizeof(uint32_t));
            partition.nodes[i].neighbors[partition.nodes[i].size-1] = partition.nodes[i].index;
        }
        
    }
}

size_t calculatePartitionArraySize(struct Graph partition) {
    uint32_t totalSize = 0;  // For size numNodes
    for (uint32_t i = 0; i < partition.numNodes; i++) {
        totalSize += 2;  // For node index and number of neighbors
        totalSize += partition.nodes[i].size;  // For the neighbors
    }
    return totalSize * sizeof(uint32_t);
}


uint32_t *constructPartitionArray(struct Graph partition, uint32_t max_size) {
    // Calculate the total size needed for the partitionArray
    size_t totalSize = calculatePartitionArraySize(partition);
    if(max_size > totalSize) totalSize = max_size;
    // Allocate memory for the partitionArray
    uint32_t *partitionArray = malloc(totalSize);

    // Start filling the partitionArray
    uint32_t currentIndex = 0;

    for (uint32_t i = 0; i < partition.numNodes; i++) {
        partitionArray[currentIndex++] = partition.nodes[i].index;
        partitionArray[currentIndex++] = partition.nodes[i].size;
        for (uint32_t j = 0; j < partition.nodes[i].size; j++) {
            partitionArray[currentIndex++] = partition.nodes[i].neighbors[j];
        }
    }
    return partitionArray;
}

/*
node0_index node0_num_neigbors neighbor0_node0 neighbor1_node0 ...
...
nodeX_index nodeX_num_neigbors neighbors0_nodeX neighbors1_nodeX ...
*/

void updateNodeLevel(uint32_t* visited, uint32_t* updatedVisited, int* nodeLevel, uint32_t nr_nodes, uint32_t curLevel) {
    for(uint32_t i=0; i<nr_nodes; i++){
        if(compareBitAt(visited, updatedVisited, i)==0){
            nodeLevel[i] = curLevel;
        }
    }
}



int main(int argc, char** argv) {
    //DPU allocation
    struct dpu_set_t dpu_set, dpu;
    uint32_t numDPUs;
    uint32_t offset;
    uint32_t xfer_size;
    Timer timer;
    for(int i=0; i<4; i++){
        timer.time[i]=0;
    }
    
    DPU_ASSERT(dpu_alloc(NR_DPUS, NULL, &dpu_set));
    DPU_ASSERT(dpu_load(dpu_set, DPU_BINARY, NULL));
    DPU_ASSERT(dpu_get_nr_dpus(dpu_set, &numDPUs));
    
    // Graph init
    //graph prepare
    int numPartitions = numDPUs;
    uint32_t source = 0;
    struct Params p = input_params(argc, argv);
    //generate_graph_file(20000, 40000, "data/nodes20000.txt");
    //p.fileName = "data/nodes15.txt";
    //p.fileName = "data/nodes120.txt";
    //p.fileName = "data/nodes20000.txt";
    struct Graph graph = readGraph(p.fileName);
    if(verbose_level) printf("Computing on CPU\n");
    int* nodeLevelCPU = BFS_CPU(graph, source);
    //struct Graph* partitions = partitionGraph(graph, numPartitions);
    struct Graph* partitions = repeatPartitionGraph(graph, numPartitions, 20);
    //printPartitions(graph, partitions, numPartitions);
    //data prepare
    unsigned int each = 0;
    uint32_t* current_partition;
    struct Parameters params[numDPUs];
    uint32_t max_size=0;
    uint32_t size;
    
    DPU_FOREACH(dpu_set, dpu, each) {
        preprocess_partition(partitions[each]);
        size = (uint32_t) calculatePartitionArraySize(partitions[each]);
        if(size>max_size) max_size = size;
        //printf("DPU %u Partition Nodes %u, Size %u \n", each, partitions[each].numNodes, params[each].partition_size);
    };
    
    start(&timer, 0);
    DPU_FOREACH(dpu_set, dpu, each) {
        preprocess_partition(partitions[each]);
        //params[each].partition_size = (uint32_t) calculatePartitionArraySize(partitions[each]);
        params[each].partition_size = max_size;
        params[each].nr_total_nodes = graph.numNodes;
        params[each].nr_nodes_partition = (uint64_t) partitions[each].numNodes;
        for(uint32_t k=0; k<NR_TASKLETS; k++) params[each].offsets[k] = 0;
        DPU_ASSERT(dpu_prepare_xfer(dpu, &params[each]));
    };
    DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, "PARAM", 0, ROUND_UP_TO_MULTIPLE_OF_8(sizeof(struct Parameters)), DPU_XFER_DEFAULT));

    uint32_t **partition_arrays = malloc(numPartitions*sizeof(uint32_t*));
    DPU_FOREACH(dpu_set, dpu, each) {
        partition_arrays[each] = constructPartitionArray(partitions[each], max_size);
    };
    DPU_FOREACH(dpu_set, dpu, each) {
        DPU_ASSERT(dpu_prepare_xfer(dpu, partition_arrays[each]));
    };
    DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, DPU_MRAM_HEAP_POINTER_NAME, 0, max_size, DPU_XFER_DEFAULT));
    stop(&timer, 0);
    printf("Size: %d\n", max_size);
    DPU_FOREACH(dpu_set, dpu, each) {
        free(partition_arrays[each]);
    };
    free(partition_arrays);

    uint32_t vectorSize = ROUND_UP_TO_MULTIPLE_OF_2((graph.numNodes + 31) / 32);
    uint32_t* frontier_visited_vector = calloc(vectorSize * 2, sizeof(uint32_t));
    uint32_t* frontier = frontier_visited_vector; 
    uint32_t* visited = frontier_visited_vector + vectorSize;
    setBitAt(frontier, source, 1);
    setBitAt(visited, source, 1);

    //BFS Algorithm
    uint32_t curLevel = 0;
    int* nodeLevel = malloc(graph.numNodes * sizeof(uint32_t));
    for (uint32_t i = 0; i < graph.numNodes; i++) {
        nodeLevel[i] = -1;  // Initialize all nodes as not reachable as -1
    }
    nodeLevel[source] = curLevel;

    while (peekFrontier(frontier_visited_vector, graph.numNodes) != -1) {
    //for(uint32_t k=0; k<2; k++){
        printf("Processing level: %u\n", curLevel);
        start(&timer, 1);
        xfer_size = ROUND_UP_TO_MULTIPLE_OF_8(vectorSize * sizeof(uint32_t) * 2);
        DPU_ASSERT(dpu_broadcast_to(dpu_set, DPU_MRAM_HEAP_POINTER_NAME, max_size, frontier_visited_vector, xfer_size, DPU_XFER_DEFAULT));
        printf("xfer_size: %d\n", xfer_size);
        stop(&timer, 1);
        if(verbose_level) printf("frontier_visited_vector sent to mram\n");
        if(verbose_level) printf("Launching DPU program\n");
        start(&timer, 2);
        DPU_ASSERT(dpu_launch(dpu_set, DPU_SYNCHRONOUS));
        stop(&timer, 2);
        if(verbose_level==2){
            DPU_FOREACH (dpu_set, dpu, each) {
                PRINT("DPU %u:", each);
                DPU_ASSERT(dpu_log_read(dpu, stdout));
                ++each;
            }
        }
        if(verbose_level) printf("DPU program terminates\n");
        uint32_t** results = malloc(numDPUs * sizeof(uint32_t*));
        DPU_FOREACH(dpu_set, dpu, each) {
            results[each] = malloc(vectorSize * sizeof(uint32_t) * 2);
        }
        start(&timer, 1);
        timer.time[3] = 0;
        start(&timer, 3);
        DPU_FOREACH(dpu_set, dpu, each) {
            DPU_ASSERT(dpu_prepare_xfer(dpu, results[each]));
        }
        DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_FROM_DPU, DPU_MRAM_HEAP_POINTER_NAME, max_size, xfer_size, DPU_XFER_DEFAULT));
        stop(&timer, 1);
        stop(&timer, 3);
        if(verbose_level) printf("DPU program results retrieved\n");
        uint32_t* mergedResult = bitVectorUnion(results, numDPUs, vectorSize * sizeof(uint32_t) * 2);
        uint32_t* updatedVisited = mergedResult + vectorSize;
        updateNodeLevel(visited, updatedVisited, nodeLevel, graph.numNodes, ++curLevel);
        memcpy(frontier_visited_vector, mergedResult, vectorSize * sizeof(uint32_t) * 2);
        if(verbose_level) printf("Next frontier obtained, Size: %u\n\n", printFrontierSize(frontier, graph.numNodes));
        //printVectorIndex(frontier, graph.numNodes);
        for (unsigned int i = 0; i < numDPUs; i++) {
            free(results[i]);
        }
        free(results);
        free(mergedResult);
    }
    if(verbose_level) printf("BFS terminated\n\n");
    //writeGraphToDotWithLevel(graph, "../graphs/DPULevelgraph.dot", nodeLevel);
    freeGraph(graph);
    free(frontier_visited_vector);
    DPU_ASSERT(dpu_free(dpu_set));
    int err = 0;
    //writeGraphToDotWithLevel(graph, "../graphs/Levelgraph.dot", nodeLevelCPU);
    if(!err) printf("Comparing results with BFS executed on CPU\n");
    for(uint32_t i=0;i<graph.numNodes;i++){
        if(nodeLevel[i]!=nodeLevelCPU[i]){
            fprintf(stderr, "[ " "\033[31m" "ERROR: Node %d mismatched, CPU level %d, DPU level: %d" "\033[0m" " ]\n", i, nodeLevelCPU[i], nodeLevel[i]);
            err = 1;
            break;
        }
    }
    if(!err) fprintf(stderr, "[ " "\033[32m" "All results matched!" "\033[0m" " ]\n");
    // Print timing results
    printf("Result: ");
    printf("\n\tCPU-DPU: ");
    print(&timer, 0, 1);
    printf("\n\tInter-DPU: ");
    print(&timer, 1, 1);
    printf("\n\tDPU: ");
    print(&timer, 2, 1);
    printf("\n\tDPU-CPU: ");
    print(&timer, 3, 1);
    printf("\n");
    return 0;
}

