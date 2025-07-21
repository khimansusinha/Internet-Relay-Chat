#ifndef _ALL_CMD_COMMON_H
#define _ALL_CMD_COMMON_H

#include "worker.h"

string ConvertToString(char *token);

void execute_cmd_nick(handler_t &h, string &args_buffer);

void execute_cmd_user(handler_t &h, string &args_buffer);

void execute_cmd_time(handler_t &h, string &args_buffer);

void execute_cmd_quit(handler_t &h, string &args_buffer, client_info_t clinfo);

void execute_cmd_privmsg(handler_t &h, string &args_buffer);

void execute_cmd_join(handler_t &h, string &args_buffer);

void execute_cmd_topic(handler_t &h, string &args_buffer);

void execute_cmd_names(handler_t &h, string &args_buffer);

void execute_cmd_part(handler_t &h, string &args_buffer);

bool lookup_nickname_map(string nick_name);

void insert_nickname_map(string nick_name, int wsfd);

#endif
