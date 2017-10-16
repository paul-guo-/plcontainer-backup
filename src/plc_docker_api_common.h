/*------------------------------------------------------------------------------
 *
 * Copyright (c) 2017-Present Pivotal Software, Inc
 *
 *------------------------------------------------------------------------------
 */


#ifndef PLC_DOCKER_API_H
#define PLC_DOCKER_API_H

#include "plc_configuration.h"
#include "plc_docker_common.h"

typedef enum {
    PLC_HTTP_GET = 0,
    PLC_HTTP_POST,
    PLC_HTTP_DELETE
} plcCurlCallType;

typedef struct {
    char   *data;
    size_t  bufsize;
    size_t  size;
    int     status;
} plcCurlBuffer;

int plc_docker_create_container(plcContainerConf *conf, char **name, int container_slot);
int plc_docker_start_container(char *name);
int plc_docker_kill_container(char *name);
int plc_docker_inspect_container(char *name, char **element, plcInspectionMode type);
int plc_docker_wait_container(char *name);
int plc_docker_delete_container(char *name);

#endif /* PLC_DOCKER_API_H */