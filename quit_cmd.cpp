#include "all_cmd_common.h"
#include <time.h>

void execute_cmd_quit(handler_t &h, string &args_buffer, client_info_t clinfo)
{
	string client_nick_name;
	string client_user_name;
	string client_hostname;
	string msg_reply = "";
	string args(args_buffer);

	//fprintf(stderr, "Inside execute_cmd_quit function args buffer is %s\n", args_buffer.c_str());

	string nick_name = ConvertToString(clinfo.nickname);

	client_nick_name = ConvertToString(clinfo.nickname);
	client_user_name = ConvertToString(clinfo.username);
	client_hostname = ConvertToString(clinfo.hostname);
	msg_reply = ":" + g_server_hostname + " QUIT " + client_nick_name + "!" + client_user_name + "@" + client_hostname;

out:
	//send final reply msg to client 
	reply_to_client(h.sockfd, msg_reply);
}
