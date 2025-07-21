#ifndef _WORKER_H
#define _WORKER_H

#include "server.h"


void parseCommand(handler &h, string &cmd_buffer, string &args_cmd);

void reply_to_client(int to_sockfd, string &reply);

void parseAndExecuteCmd(handler &h);

void* worker_loop(void *arg);
 
#endif
