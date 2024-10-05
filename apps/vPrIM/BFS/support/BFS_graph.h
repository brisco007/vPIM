#ifndef _GRAPH_H_
#define _GRAPH_H_

#include <assert.h>
#include <stdio.h>
#include "common.h"
#include "utils.h"


static struct Graph readGraph(const char* fileName) {
    FILE* file = fopen(fileName, "r");
    if (!file) {
        perror("Failed to open file");
        exit(1);
    }

    struct Graph g;
    int32_t numEdges;

    // Read number of nodes and edges
    if (fscanf(file, "%u%*[ ]%*u%*[ ]%u", &g.numNodes, &numEdges) != 2) {
        fprintf(stderr, "Error: 1 Failed to read the header.\n");
        fclose(file);
        exit(1);
    }

    g.nodes = malloc(g.numNodes * sizeof(struct Node));
    uint32_t* neighborCounts = calloc(g.numNodes, sizeof(uint32_t));

    // Temporary storage for edges to avoid multiple file reads
    uint32_t* edges = malloc(2 * numEdges * sizeof(uint32_t));

    // Read edges and count neighbors for each node
    for (int32_t i = 0; i < numEdges; i++) {
        uint32_t node, neighbor;
        if (fscanf(file, "%u%*[\t]%u", &node, &neighbor) != 2) {
            fprintf(stderr, "Error: Failed to read edge %u.\n", i);
            fclose(file);
            exit(1);
        }
        edges[2 * i] = node;
        edges[2 * i + 1] = neighbor;
        neighborCounts[node]++;
    }

    // Allocate space for neighbors based on the counts
    for (uint32_t i = 0; i < g.numNodes; i++) {
        g.nodes[i].index = i;
        g.nodes[i].size = neighborCounts[i];
        g.nodes[i].neighbors = (neighborCounts[i] > 0) ? malloc(neighborCounts[i] * sizeof(uint32_t)) : NULL;
        neighborCounts[i] = 0;  // Reset for next pass
    }

    // Store the neighbors
    for (int32_t i = 0; i < numEdges; i++) {
        uint32_t node = edges[2 * i];
        uint32_t neighbor = edges[2 * i + 1];
        g.nodes[node].neighbors[neighborCounts[node]++] = neighbor;
    }

    free(neighborCounts);
    free(edges);
    fclose(file);

    return g;
}



static void freeGraph(struct Graph g) {
    for (uint32_t i = 0; i < g.numNodes; i++) {
        free(g.nodes[i].neighbors);
    }
    free(g.nodes);
}

uint32_t computeGraphSize(struct Graph g) {
    uint32_t totalSize = 0;
    for (uint32_t i = 0; i < g.numNodes; i++) {
        totalSize += 1 + g.nodes[i].size; // 1 for the node itself and size for its neighbors
    }
    return totalSize;
}

uint32_t random_int(uint32_t min, uint32_t max) {
   return min + rand() % (max - min);
}

struct Graph* partitionGraph(struct Graph g, uint32_t numPartitions) {
    if (numPartitions == 0) return NULL;

    uint32_t totalSize = computeGraphSize(g);
    uint32_t averageSize = totalSize / numPartitions;
    struct Graph* partitions = (struct Graph*)malloc(sizeof(struct Graph) * numPartitions);
    if (!partitions) return NULL;

    uint32_t currentStart = 0;
    for (uint32_t i = 0; i < numPartitions; i++) {
        partitions[i].numNodes = 0;
        partitions[i].nodes = (struct Node*)malloc(sizeof(struct Node) * g.numNodes); // allocate max possible size, will trim later
        uint32_t currentPartitionSize = 0;
        while (currentStart < g.numNodes) {
            partitions[i].nodes[partitions[i].numNodes++] = g.nodes[currentStart];
            currentPartitionSize += 1 + g.nodes[currentStart].size;
            currentStart++;
            if (currentPartitionSize > averageSize) break; 
        }
        while(currentStart >= g.numNodes && currentPartitionSize < averageSize){
            uint32_t random_index = random_int(0, g.numNodes);
            partitions[i].nodes[partitions[i].numNodes++] = g.nodes[random_index];
             currentPartitionSize += 1 + g.nodes[random_index].size;
        }

        // Trim the nodes array to the actual size
        partitions[i].nodes = (struct Node*) realloc(partitions[i].nodes, sizeof(struct Node) * partitions[i].numNodes);
    }

    return partitions;
}

struct Graph* repeatPartitionGraph(struct Graph g, uint32_t numPartitions, int size){
    if(numPartitions < size){
        return partitionGraph(g, numPartitions);
    }
    struct Graph* partitions = (struct Graph*) malloc(sizeof(struct Graph) * numPartitions);
    struct Graph* real_partitions = (struct Graph*) malloc(sizeof(struct Graph) * size);
    real_partitions = partitionGraph(g, size);
    for(int i=0; i<numPartitions; i++){
        partitions[i] = real_partitions[i%size];
    }
    return partitions;
}

typedef struct {
    int start;
    int end;
} Edge;

int compare_edges(const void* a, const void* b) {
    Edge* edgeA = (Edge*)a;
    Edge* edgeB = (Edge*)b;

    if (edgeA->start == edgeB->start) {
        return edgeA->end - edgeB->end;
    }
    return edgeA->start - edgeB->start;
}

int is_edge_existing(int node1, int node2, Edge* edges, int edges_added) {
    for (int i = 0; i < edges_added; i++) {
        if ((edges[i].start == node1 && edges[i].end == node2) || 
            (edges[i].start == node2 && edges[i].end == node1)) {
            return 1;
        }
    }
    return 0;
}

void generate_graph_file(int numNodes, int numEdges, const char* filePath) {
    FILE* file = fopen(filePath, "w");
    if (!file) {
        perror("Failed to open file for writing");
        exit(1);
    }

    // Seed random number generator
    srand(10);

    // Write header
    fprintf(file, "%d %d %d\n", numNodes, numNodes, numEdges * 2);  // Multiply by 2 for both edges

    int edgesAdded = 0;
    Edge* edges = malloc(numEdges * 2 * sizeof(Edge));  // Allocate space for both edges

    // Start by creating a spanning tree to ensure connectivity
    for (int i = 1; i < numNodes && edgesAdded < numEdges * 2; i++) {
        int node1 = i;
        int node2 = rand() % i;  // Ensure a node from 0 to i-1

        edges[edgesAdded].start = node1;
        edges[edgesAdded].end = node2;
        edgesAdded++;

        // Add the reverse edge for undirected graph
        edges[edgesAdded].start = node2;
        edges[edgesAdded].end = node1;
        edgesAdded++;
    }

    // Add additional edges until the desired number is reached
    while (edgesAdded < numEdges * 2) {
        int node1 = rand() % numNodes;
        int node2 = rand() % numNodes;

        if (node1 != node2 && 
            !is_edge_existing(node1, node2, edges, edgesAdded) && 
            !is_edge_existing(node2, node1, edges, edgesAdded)) {
            edges[edgesAdded].start = node1;
            edges[edgesAdded].end = node2;
            edgesAdded++;

            // Add the reverse edge for undirected graph
            edges[edgesAdded].start = node2;
            edges[edgesAdded].end = node1;
            edgesAdded++;
        }
    }

    // Sort the edges
    qsort(edges, edgesAdded, sizeof(Edge), compare_edges);

    // Write sorted edges to file
    for (int i = 0; i < edgesAdded; i++) {
        fprintf(file, "%d\t%d\n", edges[i].start, edges[i].end);
    }

    free(edges);
    fclose(file);
}

void printPartitions(struct Graph* partitions, uint32_t numPartitions) {
    // Now, print each partition
    for (uint32_t p = 0; p < numPartitions; p++) {
        printf("Partition %u:\n", p);
        printf("-----------------\n");
        printf("Total number of nodes: %u\n", partitions[p].numNodes);
        for (uint32_t i = 0; i < partitions[p].numNodes; i++) {
            printf("Node %u: ", partitions[p].nodes[i].index);
            for (uint32_t j = 0; j < partitions[p].nodes[i].size; j++) {
                printf("%u ", partitions[p].nodes[i].neighbors[j]);
            }
            printf("\n");
        }
        printf("\n");
    }
}


void writeGraphToDot(struct Graph g, const char* filePath) {
    FILE* file = fopen(filePath, "w");
    if (!file) {
        perror("Failed to open file for writing");
        exit(1);
    }

    fprintf(file, "graph G {\n");

    for (uint32_t i = 0; i < g.numNodes; i++) {
        for (uint32_t j = 0; j < g.nodes[i].size; j++) {
            if (i < g.nodes[i].neighbors[j]) {
                fprintf(file, "    %u -- %u;\n", i, g.nodes[i].neighbors[j]);
            }
        }
    }

    fprintf(file, "}\n");
    fclose(file);
}


void writeGraphToDotWithLevel(struct Graph g, const char* filePath, int* nodeLevel) {
    FILE* file = fopen(filePath, "w");
    if (!file) {
        perror("Failed to open file for writing?");
        exit(1);
    }

    fprintf(file, "graph G {\n");

    for (uint32_t i = 0; i < g.numNodes; i++) {
        fprintf(file, "    %u [label=\"%u (Level %d)\"];\n", i, i, nodeLevel[i]);
        for (uint32_t j = 0; j < g.nodes[i].size; j++) {
            if (i < g.nodes[i].neighbors[j]) {
                fprintf(file, "    %u -- %u;\n", i, g.nodes[i].neighbors[j]);
            }
        }
    }

    fprintf(file, "}\n");
    fclose(file);
}


void writePartitionsToDot(struct Graph* partitions, uint32_t numPartitions, const char* fileName) {
    FILE* file = fopen(fileName, "w");
    if (!file) {
        perror("Failed to open file for writing");
        exit(1);
    }

    fprintf(file, "graph Partitions {\n");  // Start the main graph

    for (uint32_t p = 0; p < numPartitions; p++) {
        fprintf(file, "    subgraph cluster%u {\n", p);  // Start a subgraph for each partition
        fprintf(file, "        label=\"Partition %u\";\n", p);

        // Explicitly list all nodes
        for (uint32_t i = 0; i < partitions[p].numNodes; i++) {
            uint32_t currentNodeIndex = partitions[p].nodes[i].index;
            fprintf(file, "        %u;\n", currentNodeIndex);
        }

        // List edges
        for (uint32_t i = 0; i < partitions[p].numNodes; i++) {
            uint32_t currentNodeIndex = partitions[p].nodes[i].index;
            for (uint32_t j = 0; j < partitions[p].nodes[i].size; j++) {
                uint32_t neighborIndex = partitions[p].nodes[i].neighbors[j];
                if (currentNodeIndex < neighborIndex) {  // Avoid duplicate edges for undirected graph
                    fprintf(file, "        %u -- %u;\n", currentNodeIndex, neighborIndex);
                }
            }
        }

        fprintf(file, "    }\n");  // Close the current subgraph
    }

    fprintf(file, "}\n");  // Close the main graph

    fclose(file);
}


#endif