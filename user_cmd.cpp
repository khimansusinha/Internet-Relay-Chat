#include "all_cmd_common.h"

void execute_cmd_user(handler_t &h, string &args_buffer)
{
	char *token = NULL;
	string args(args_buffer);
	int count = 0;
	string user_name = "";
	string real_name = "";
	string user_mode = "";
	string user_unused = "";
	string msg_reply = "";
	char beforecolonstr[1024];
	string client_hostname;

	//fprintf(stderr, "Inside execute_cmd_user function args buffer is %s\n", args_buffer.c_str());

	//now clinet info from  global server worker socket fd and client info map
        pthread_rwlock_rdlock(&map_clinfo_rw_lock);
        client_info_t clinfo;
        clinfo = g_ws_clinfo_map[h.sockfd];
        pthread_rwlock_unlock(&map_clinfo_rw_lock);

	string nick_name = ConvertToString(clinfo.nickname);

	if (clinfo.nickusercmd[0] != true) {
		//451    ERR_NOTREGISTERED
		msg_reply = ":" + g_server_hostname + "451 " + ":You have not registered";
		goto out;
	}

	//don't allow to change the user name, only nick name is allowed to change
	if (clinfo.nickusercmd[1] == true) {
		//462    ERR_ALREADYREGISTRED
		msg_reply = ":" + g_server_hostname + " 462 " + ":Unauthorized command (already registered)";
		goto out;
	}

	//user command-> USER guest 0 * :real name
	count = 0;
	token = strtok((char*)args.c_str(), ":\n");
	if (token != NULL) //fprintf(stderr, "the token is %s\n", token);

	while (token != NULL) {
		memcpy(beforecolonstr, token, strlen(token) + 1);
		count++;
		//fprintf(stderr, "The before colon string is %s\n", beforecolonstr);
		token = strtok(NULL, "\n\r");
		if (token == NULL) {
			//fprintf(stderr, "the token is null\n");
			break;
		}
		real_name = ConvertToString(token);
		//fprintf(stderr, "the second token real name is %s\n", real_name.c_str());
		count++;
		break;
	}
	if (count != 2) {
		msg_reply = ":" + g_server_hostname + " 461 " + "USER :Not enough parameters";
		goto out;
	}

	count = 0;
	//now parse the token before colon to get the user name
	token = strtok(beforecolonstr, " \n\r");
	while (token != NULL) {
		user_name = ConvertToString(token);
		//fprintf(stderr, "The user name given is: %s\n", user_name.c_str());
		count++;

		token = strtok(NULL, " \n\r");
		if (token == NULL) {
			//fprintf(stderr, "error in usermode token\n");
			break;
		}
		user_mode = ConvertToString(token);
		//fprintf(stderr, "the user mode is: %s\n", user_mode.c_str());
		count++;

		token = strtok(NULL, " \n\r");
		if (token == NULL) {
			//fprintf(stderr, "error in user unused token\n");
			break;
		}
		user_unused = ConvertToString(token);
		//fprintf(stderr, "The user unused value is: %s\n", user_unused.c_str());
		count++;

		break;
	}
	
	if (count != 3) {
		//461    ERR_NEEDMOREPARAMS
		msg_reply = ":" + g_server_hostname + " 461 " + "USER :Not enough parameters";
		goto out;
	}

	//now insert this user name inside the global server worker socket fd and client info map
	pthread_rwlock_wrlock(&map_clinfo_rw_lock);
	memcpy(clinfo.username, user_name.c_str(), user_name.size());
	memcpy(clinfo.usermode, user_mode.c_str(), user_mode.size());
	memcpy(clinfo.userunused, user_unused.c_str(), user_unused.size());
	clinfo.nickusercmd[1] = true;
	g_ws_clinfo_map[h.sockfd] = clinfo;
	pthread_rwlock_unlock(&map_clinfo_rw_lock);

	client_hostname = ConvertToString(clinfo.hostname);
	//fprintf(stderr, "the client hostname stored in hash map for sockfd [%d] is [%s]\n",h.sockfd, clinfo.hostname);
	msg_reply = ":" + g_server_hostname + " 001 " + nick_name + " :Welcome to the Internet Relay Network " + nick_name + "!" + user_name + "@" + client_hostname;

out:
	//send final reply msg to client 
	reply_to_client(h.sockfd, msg_reply);
}
