#include "utils.h"

#ifndef TRANSPORT_H
#define TRANSPORT_H

//Serialize request string
char* serializeRequest( Request* request);
//Serialize response 
char* serializeResponse( Response* response);
//Deserialize request struct
Request* deserializeRequest( char* serializedRequest);
//Deserialize response
Response* deserializeResponse( char* serializedResponse);

#endif