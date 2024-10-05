#ifndef HANDLERS_H
#define HANDLERS_H

#include "./utils.h"
#include <stdio.h>

Response* handle_alloc_req( Request* req);
Response* handle_free_req( Request* req);

#endif
