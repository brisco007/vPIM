#ifndef OBSERVER_H
#define OBSERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "utils.h"

// Déclaration externe de la variable path_list

// Durée de la pause en microsecondes

// Fonction pour mettre à jour le statut d'un rang dans la table
void update_rank_status(int rank_id, int is_owned);

// Fonction pour observer la variation des fichiers is_owned
void* observe_is_owned(void* nr_ranks);

void fill_path_list(int nr_ranks);
#endif 