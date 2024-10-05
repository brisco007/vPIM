#include "utils.h"
#include <fcntl.h>
#include <sys/mman.h>

#ifndef TABLE_MANAGEMENT_H
#define TABLE_MANAGEMENT_H

#define RANK_IS_OWNED_PATH "/sys/class/dpu_rank/dpu_rank%d/is_owned"
#define RANK_DAX_PATH "/dev/dax%d.%d"
#define SEARCH_DELAY_MS 180000 //3 minutes
#define MAX_SEARCH_ITERATIONS 100
#define FILE_SIZE 8589934592

extern int nb_ranks;
extern int nb_free_ranks;
VPIM_t** head_vpim; 

int count_files();
int init_rank_table();
int check_is_owned(int index);
int inc_free();
int dec_free();
int get_free();
int set_free(int value);
int gen_vpim_id();
void zero_the_rank(Entry* entry);
int get_rank_id(int index);
int get_rank_index(int index);
Entry* find_first_available();
int update_vpim_id(int rank_id, unsigned int new_vpim_id);
int update_is_owned(int rank_id, int new_is_owned);
 void insert_node(VPIM_t **head, unsigned int vpim_id, unsigned int rank_assigned);
 void init_list_head();

VPIM_t *find_node(VPIM_t *head, unsigned int vpim_id);
VPIM_t *find_node_and_update(VPIM_t *head, unsigned int vpim_id, unsigned int rank_assigned);
void print_list(VPIM_t *head) ;
//We should be able to execute this request : 
/*
 curl --unix-socket /tmp/firecracker.socket -i \
  -X PATCH 'http://localhost/vpim' \
  -H 'Accept: application/json' \
  -H 'Content-Type: application/json' \
  -d '{
    }'

*/

#endif