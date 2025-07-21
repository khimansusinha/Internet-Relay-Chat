#include "all_cmd_common.h"
#include <time.h>

void execute_cmd_time(handler_t &h, string &args_buffer)
{
	string msg_reply = "";
	char timebuff[1024];
	string strtime = "";
	string args(args_buffer);

	//fprintf(stderr, "Inside execute_cmd_time function args buffer is %s\n", args_buffer.c_str());

	//now clinet info from  global server worker socket fd and client info map
        pthread_rwlock_rdlock(&map_clinfo_rw_lock);
        client_info_t clinfo;
        clinfo = g_ws_clinfo_map[h.sockfd];
        pthread_rwlock_unlock(&map_clinfo_rw_lock);

	string nick_name = ConvertToString(clinfo.nickname);

	if ((clinfo.nickusercmd[0] != true) && (clinfo.nickusercmd[1] != true)) {
		//451    ERR_NOTREGISTERED
		msg_reply = ":" + g_server_hostname + "451 " + ":You have not registered";
		goto out;
	}
	if (args != "") {
		msg_reply = ":" + g_server_hostname + " 461 " + "USER :Not enough parameters";
		goto out;
	}
	time_t rawtime;
	struct tm *l_time;

	time(&rawtime);
	l_time = localtime(&rawtime);
	//fprintf(stderr, "the server local time and date is [%s]\n", asctime(l_time));

	//snprintf(timebuff, sizeof(timebuff), "%s", (char*)l_time);
	strtime = ConvertToString(asctime(l_time));
	//391    RPL_TIME
	msg_reply = ":" + g_server_hostname + " 391 " + g_server_hostname + " :" + strtime;

out:
	//send final reply msg to client 
	reply_to_client(h.sockfd, msg_reply);
}
