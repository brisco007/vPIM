#include "../include/handlers.h"
#include "../include/table_management.h"

#define NO_RANK_AVAILABLE = "No rank"
#define RANK_ERROR 99
#define VPIM_DEFAULT_ID 99
Entry* rank_table;

Response* handle_alloc_req(Request* req) {
    // TODO: Implémenter la logique de traitement pour l'allocation de rank
    //printf("called handler alloc\n");
    Entry* rank = NULL;
    //rank = find_first_available();
    Response* res = (Response*)malloc(sizeof(Response));

    if(req->vpim_id == VPIM_DEFAULT_ID){
        rank = find_first_available();
        if(rank != NULL) {
            res->req_type = req->req_type;
            res->status = RES_OK;
            res->req_len = 0;
            res->rank_path = rank->rank_path;
            res->dax_path = rank->dax_path;
            res->rank_id = rank->rank_id;
            res->vpim_id =  gen_vpim_id();
            insert_node(head_vpim,res->vpim_id,rank->rank_id);
            pthread_mutex_lock(&rank_table_mutex);
            rank->is_owned = 1;
            pthread_mutex_unlock(&rank_table_mutex); 
        } else {
            //not tested
            res->req_type = req->req_type;
            res->status = RES_UNAVAILABLE;
            res->req_len = 0;
            res->rank_path = "No rank";
            res->dax_path = "No rank";
            res->rank_id = RANK_ERROR;
            res->vpim_id = VPIM_DEFAULT_ID;
        }
    } else {
        print_list(*head_vpim);
        VPIM_t* entry = find_node(*head_vpim, req->vpim_id);
        if (entry != NULL) {
            int index = get_rank_index(entry->rank_assigned);
            int is_owned = rank_table[index].is_owned;
            if (is_owned == AVAILABLE || is_owned == RESET_STATE) {
                pthread_mutex_lock(&rank_table_mutex);
                rank_table[index].is_owned = 1;
                pthread_mutex_unlock(&rank_table_mutex); 
                res->req_type = req->req_type;
                res->status = RES_OK;
                res->req_len = 0;
                res->rank_path = rank_table[index].rank_path;
                res->dax_path = rank_table[index].dax_path;
                res->rank_id = rank_table[index].rank_id;

            } else {
                //not tested
                rank = find_first_available();
                if(rank != NULL) {
                    res->req_type = req->req_type;
                    res->status = RES_OK;
                    res->req_len = 0;
                    res->rank_path = rank->rank_path;
                    res->dax_path = rank->dax_path;
                    res->rank_id = rank->rank_id;
                    find_node_and_update(*head_vpim,res->vpim_id,rank->rank_id);
                    pthread_mutex_lock(&rank_table_mutex);
                    rank->is_owned = 1;
                    pthread_mutex_unlock(&rank_table_mutex); 
                } else {
                    res->req_type = req->req_type;
                    res->status = RES_UNAVAILABLE;
                    res->req_len = 0;
                    res->rank_path = "No rank";
                    res->dax_path = "No rank";
                    res->rank_id = RANK_ERROR;
                    res->vpim_id = VPIM_DEFAULT_ID;
                }
            }
        } else {
            res->req_type = req->req_type;
            res->status = RES_FAILED;
            res->req_len = 0;
            res->rank_path = "No rank";
            res->dax_path = "No rank";
            res->rank_id = RANK_ERROR;
            res->vpim_id = req->req_type;
        }
    }    
    return res; 
}

Response* handle_free_req(Request* req) {
    // TODO: Implémenter la logique de traitement pour la libération de rank
    printf("called handler free\n");
    Response* res = (Response*)malloc(sizeof(Response));
            res->req_type = req->req_type;
            res->status = RES_UNAVAILABLE;
            res->req_len = 0;
            res->vpim_id = req->vpim_id;
            res->rank_path = "No rank";
            res->dax_path = "No rank";
            res->rank_id = 2;
    return res; // Remplacer NULL par le résultat approprié
}
