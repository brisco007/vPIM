#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "../include/table_management.h"

int nb_ranks; 
int nb_free_ranks;
unsigned int vpim_id;
char** path_list;

Entry* rank_table;



int inc_free() {
    ++nb_free_ranks;
     return nb_free_ranks;
}
int dec_free(){   
    --nb_free_ranks;
     return nb_free_ranks;
}
int get_free() {
    return nb_free_ranks;
}
int set_free(int value) {
    nb_free_ranks = value;
    return nb_free_ranks;
}
int gen_vpim_id() {
    return vpim_id++;
}

int count_files() {
    int files = 0;
    DIR* folder = opendir(DEV_PATH);
    struct dirent *entry;

    if(folder == NULL){
        printf("Unable to read directory");
        return -1;
    }


    while( (entry=readdir(folder)) )
    {
        if (strstr(entry->d_name, "dpu_rank") != NULL) {
            files++;
        }
        
    }
    closedir(folder);

    return files;
}

int check_is_owned(int index) {
    // Construire le chemin du fichier en utilisant le template
    char filepath[sizeof(RANK_IS_OWNED_PATH) + 2]; // +1 pour le caractère nul
    sprintf(filepath, RANK_IS_OWNED_PATH, index);

    // Ouvrir le fichier en mode lecture
    FILE* file = fopen(filepath, "r");
    if (file == NULL) {
        printf("Erreur lors de l'ouverture du fichier.\n");
        exit(1);
    }

    // Lire l'entier depuis le fichier
    int value;
    fscanf(file, "%d", &value);

    // Fermer le fichier
    fclose(file);

    // Retourner la valeur lue
    return value;
}
int get_rank_id(int index){
    return nb_ranks -1 - index;
}
int get_rank_index(int rank_id){
    return nb_ranks - 1 - rank_id;
}
int init_rank_table() {
    DIR* folder = opendir(DEV_PATH);
    struct dirent *entry;
    int index = 0;
    if(folder == NULL){
        printf("Unable to read directory");
        return -1;
    }
    nb_ranks = 0;
    nb_ranks = count_files();
    set_free(nb_ranks);
    rank_table = (Entry*) malloc(sizeof(Entry) * nb_ranks);

    while( (entry=readdir(folder)) )
    {
        if (strstr(entry->d_name, "dpu_rank") != NULL) {
            int rank_id = entry->d_name[8] - '0';
            int is_owned = check_is_owned(rank_id);
            if(is_owned == 1) {
                dec_free();
            }
            char * path = (char*) malloc(sizeof(15+1));
            strcpy(path, DEV_PATH);
            strcat(path, entry->d_name);
            rank_table[index] = (Entry) {
                .rank_path =  path,
                .dax_path = (char*) malloc(sizeof(RANK_DAX_PATH +1)),
                .is_owned = is_owned,
                .vpim_id = -1,
                .rank_id = rank_id
            };
            sprintf(rank_table[index].dax_path, RANK_DAX_PATH, rank_id,rank_id);
            printf("My entry[%d]:\n path : %s,  dax_path: %s,    is_owned: %d,  vpim_id : %d, rank_id : %d\n\n", index,
            rank_table[index].rank_path,
            rank_table[index].dax_path,
            rank_table[index].is_owned,
            rank_table[index].vpim_id,
            rank_table[index].rank_id);
            index++;
        }
    }
    closedir(folder);

    return 0;

}


Entry* find_first_available() {
    static int last_used_rank_index = 0; 
    int last_used_index = nb_ranks- 1 - last_used_rank_index; 
    
    // Pas trouvé, effectue des recherches supplémentaires
    /* for (int iter = 0; iter < MAX_SEARCH_ITERATIONS; iter++) {
        usleep(SEARCH_DELAY_MS * 1000); // Attente en microsecondes
        for (int i = nb_ranks-1; i >=0 ; i--) {
            if (rank_table[i].is_owned == 0) {
                return &(rank_table[i]); // Trouvé, renvoie le pointeur vers l'entrée
            }
        }
    } */

    for (int iter = 0; iter < MAX_SEARCH_ITERATIONS; iter++) {
        usleep(SEARCH_DELAY_MS * 1000); // Attente en microsecondes

        // Recherche à partir de l'index du dernier rank utilisé
        for (int i = last_used_index; i >= 0; i--) {
            if (rank_table[i].is_owned == 0) {
                last_used_rank_index++; // Met à jour l'index du dernier rank utilisé
                return &(rank_table[i]); // Trouvé, renvoie le pointeur vers l'entrée
            }
        }

        // Si aucun rank n'est trouvé à partir de l'index du dernier rank utilisé,
        // recherche depuis le dernier index jusqu'au début du tableau
        for (int i = nb_ranks - 1; i > last_used_index; i--) {
            if (rank_table[i].is_owned == 0) {
                last_used_rank_index++; // Met à jour l'index du dernier rank utilisé
                return &(rank_table[i]); // Trouvé, renvoie le pointeur vers l'entrée
            }
        }
    }
    return NULL; // Aucun `rank` disponible trouvé
}

int update_vpim_id(int rank_id, unsigned int new_vpim_id) {
    for (int i = 0; i < nb_ranks; i++) {
        if (rank_table[i].rank_id == rank_id) {
            rank_table[i].vpim_id = new_vpim_id;
            return 0; // Succès
        }
    }

    return -1; // Aucune correspondance trouvée
}

int update_is_owned(int rank_id, int new_is_owned) {
    for (int i = 0; i < nb_ranks; i++) {
        if (rank_table[i].rank_id == rank_id) {
            rank_table[i].is_owned = new_is_owned;
            return 0; // Succès
        }
    }

    return -1; // Aucune correspondance trouvée
}

void zero_the_rank(Entry* entry) {
    int fd = open(entry->dax_path, O_RDWR);
    if (fd == -1) {
        printf("Erreur lors de l'ouverture du fichier.\n");
        exit(1);
    }

    size_t file_size = FILE_SIZE;

    void* addr = mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        printf("Erreur lors du mmap de la mémoire.\n");
        exit(1);
    }

    memset(addr, 0, file_size); // Remplit la mémoire avec des zéros
    //printf("reset done\n");
    if (munmap(addr, file_size) == -1) {
        printf("Erreur lors de l'unmapping de la mémoire.\n");
        exit(1);
    }

    close(fd);
}

//VPIM list

 void insert_node(VPIM_t **head, unsigned int vpim_id, unsigned int rank_assigned) {
    VPIM_t *new_node = malloc(sizeof(VPIM_t));
    new_node->vpim_id = vpim_id;
    new_node->rank_assigned = rank_assigned;
    //new_node->unix_socket = strdup(unix_socket);
    new_node->next = *head;
    *head = new_node;
    //print_list(*head);
}

 void init_list_head() {
    if(head_vpim == NULL) {
        head_vpim = (VPIM_t **) malloc(sizeof(VPIM_t*));
        *head_vpim = NULL;
    }
}

// Fonction pour rechercher un élément dans la liste à partir de son vpim_id
VPIM_t *find_node(VPIM_t *head, unsigned int vpim_id) {
    VPIM_t *current = head;
    while (current != NULL) {
        if (current->vpim_id == vpim_id) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// Fonction pour rechercher un élément dans la liste à partir de son vpim_id
VPIM_t *find_node_and_update(VPIM_t *head, unsigned int vpim_id, unsigned int rank_assigned) {
    VPIM_t *current = head;
    while (current != NULL) {
        if (current->vpim_id == vpim_id) {
            current->rank_assigned = rank_assigned;
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// Fonction pour afficher tous les éléments de la liste
void print_list(VPIM_t *head) {
    VPIM_t *current = head;
    printf("List of vPIMs:\n");
    while (current != NULL) {
        printf("vpim_id: %u,\n", current->vpim_id);
        current = current->next;
    }
    printf("printing done \n");
}
