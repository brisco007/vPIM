// common.h
#ifndef COMMON_H
#define COMMON_H

#define MAX_DATA_SIZE 1024

typedef struct {
    int id;
    char data[MAX_DATA_SIZE];
} Req;

#endif  // COMMON_H