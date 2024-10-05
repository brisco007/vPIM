#include "../include/observer.h"
#include "../include/table_management.h"
#include <time.h>
#include <stdlib.h>

char** path_list;
Entry* rank_table;
// Durée de la pause en microsecondes

pthread_mutex_t rank_table_mutex;
#define PAUSE_DURATION_US 100

// Fonction qui crée un chemin à partir d'un ID de rang
char* create_path_from_rank_id(int rank_id) {
    char* path = (char*)malloc((sizeof(RANK_IS_OWNED_PATH) +2) * sizeof(char));
    sprintf(path, RANK_IS_OWNED_PATH, rank_id);
    return path;
}


// Fonction qui remplit la liste de chemins pour tous les rank_id disponibles
void fill_path_list(int nr_ranks) {
    path_list = (char**)malloc(nr_ranks * sizeof(char*));
    for (int rank_id = 0; rank_id < nr_ranks; rank_id++) {
        path_list[rank_id] = create_path_from_rank_id(rank_id);
    }
}

// Fonction pour mettre à jour le statut d'un rang dans la table
void update_rank_status(int index, int is_owned) {
    // Mettez ici la logique pour mettre à jour la table des rangs
    // en utilisant les valeurs de rank_id et is_owned
    //struct timespec start, end;
    //double elapsed;
    int rank_id = get_rank_id(index);
    int old = rank_table[rank_id].is_owned;
    if(is_owned == 0) {
        pthread_mutex_lock(&rank_table_mutex);
        rank_table[rank_id].is_owned = RESET_STATE;
        pthread_mutex_unlock(&rank_table_mutex);
    
        //clock_gettime(CLOCK_MONOTONIC, &start);
        zero_the_rank(&rank_table[rank_id]);
        //clock_gettime(CLOCK_MONOTONIC, &end);
    } 
    pthread_mutex_lock(&rank_table_mutex);
    rank_table[rank_id].is_owned = is_owned;
    pthread_mutex_unlock(&rank_table_mutex);
    printf("Updating rank status: Rank %d was %d and now is = %d\n", index, old, is_owned);
    /* elapsed = (end.tv_sec - start.tv_sec)*1000000 + (end.tv_nsec - start.tv_nsec) / 1000.0;
    printf("Execution time zero the rank : %.10f microseconds\n", elapsed); */
}

// Fonction pour observer la variation des fichiers is_owned
void* observe_is_owned(void* arg) {
    int nr_ranks = *((int *) arg);
    int* previous_values = (int*)malloc(nr_ranks * sizeof(int));
    
    // Boucle d'observation des fichiers is_owned
    while (1) {
        // Lecture des fichiers is_owned
        for (int rank_id = 0; rank_id < nr_ranks; rank_id++) {
            char* path = path_list[rank_id];
            FILE* file = fopen(path, "r");
            if (file == NULL) {
                printf("Erreur lors de l'ouverture du fichier.\n");
                exit(1);
            }

            // Lecture du contenu du fichier
            int is_owned;
            ssize_t bytes_read = fscanf(file, "%d", &is_owned);
            fclose(file);
            if (bytes_read == -1) {
                printf("Error reading file: %s\n", path);
                continue;
            }
            // Vérification de la variation et mise à jour de la table des rangs
            if (is_owned != previous_values[rank_id]) {
                update_rank_status(rank_id, is_owned);
            }

            // Stockage de la valeur actuelle pour la prochaine itération
            previous_values[rank_id] = is_owned;
        }

        // Pause pour observer périodiquement les variations
        usleep(PAUSE_DURATION_US);
    }

    free(previous_values);
    return NULL;
}