#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/transport.h"

char* serializeRequest( Request* request) {
    // Calculer la taille requise pour la chaîne de caractères
    size_t serializedSize = snprintf(NULL, 0, "%u,%u,%u",
                                     request->req_type, request->req_len,
                                     request->vpim_id);
    
    // Allouer la mémoire pour la chaîne de caractères
    char* serializedRequest = (char*)malloc(serializedSize + 1);
    if (serializedRequest == NULL) {
        // Gestion de l'erreur d'allocation mémoire
        return NULL;
    }
    
    // Sérialiser la structure Request dans la chaîne de caractères
    snprintf(serializedRequest, serializedSize + 1, "%u,%u,%u",
             request->req_type, request->req_len, request->vpim_id);
    
    return serializedRequest;
}

char* serializeResponse( Response* response) {
    // Calculer la taille requise pour la chaîne de caractères
    size_t serializedSize = snprintf(NULL, 0, "%u,%u,%u,%u,%s,%s,%u",
                                     response->req_type,response->status, response->req_len,
                                     response->vpim_id, response->rank_path,response->dax_path,
                                     response->rank_id);
    response->req_len=serializedSize;
    // Allouer la mémoire pour la chaîne de caractères
    char* serializedResponse = (char*)malloc(serializedSize + 2);
    if (serializedResponse == NULL) {
        // Gestion de l'erreur d'allocation mémoire
        return NULL;
    }
    
    // Sérialiser la structure Response dans la chaîne de caractères
    snprintf(serializedResponse, serializedSize + 2, "%u,%u,%u,%u,%s,%s,%u",
             response->req_type,response->status, response->req_len, response->vpim_id,
             response->rank_path,response->dax_path, response->rank_id);
    serializedResponse[serializedSize+1]='$';
    printf("%s\n",serializedResponse);
    return serializedResponse;
}

Request* deserializeRequest(char* serializedRequest) {
    // Allouer la mémoire pour la structure Request
    Request* request = (Request*)malloc(sizeof(Request));
    if (request == NULL) {
        // Gestion de l'erreur d'allocation mémoire
        return NULL;
    }
    
    // Utiliser la fonction sscanf pour désérialiser la chaîne de caractères
    int result = sscanf(serializedRequest, "%u,%u,%u",
                        &request->req_type, &request->req_len, &request->vpim_id);
    if (result != 3) {
        // Gestion de l'erreur de désérialisation
        free(request);
        return NULL;
    }
    
    return request;
}

Response* deserializeResponse( char* serializedResponse) {
    // Allouer la mémoire pour la structure Response
    Response* response = (Response*)malloc(sizeof(Response));
    if (response == NULL) {
        // Gestion de l'erreur d'allocation mémoire
        return NULL;
    }
    
    // Utiliser la fonction sscanf pour désérialiser la chaîne de caractères
    int result = sscanf(serializedResponse, "%u,%u,%u,%u,%m[^,],%m[^,],%u",
                        &response->req_type,&response->status, &response->req_len, &response->vpim_id,
                        &response->rank_path,&response->dax_path, &response->rank_id);
    
    if (result != 7) {
        // Gestion de l'erreur de désérialisation
        free(response);
        return NULL;
    }
    
    return response;
}
