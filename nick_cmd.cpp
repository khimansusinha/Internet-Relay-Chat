#include "all_cmd_common.h"

void execute_cmd_nick(handler_t &h, string &args_buffer)
{
	char *token = NULL;
	string args(args_buffer);
	int count = 0;
	string nick_name = "";
	string msg_reply = "";

	//fprintf(stderr, "Inside execute_cmd_nick function args buffer is %s\n", args_buffer.c_str());

	token = strtok((char*)args.c_str(), " \n\r");
	if (token != NULL) {
		//fprintf(stderr, "the 1st token is %s\n", token);
		count++;
	}
	while (token != NULL) {
		nick_name = ConvertToString(token);
		//fprintf(stderr, "The nick name is %s\n", nick_name.c_str());
		token = strtok(NULL, " \n\r");
		if (token == NULL) {
			//fprintf(stderr, "the token is null\n");
			break;
		}
		//fprintf(stderr, "the token is %s\n", token);
		count++;
		break;
	}

	//now clinet info from  global server worker socket fd and client info map
        pthread_rwlock_rdlock(&map_clinfo_rw_lock);
        client_info_t clinfo = {0};
        clinfo = g_ws_clinfo_map[h.sockfd];
        pthread_rwlock_unlock(&map_clinfo_rw_lock);

	if (count != 1) {
		//send reply to same client invalid number of arguments for NICK command
		if (count == 0) {
			//431    ERR_NONICKNAMEGIVEN
			msg_reply = g_server_hostname + " 431 * * " + " :No nickname given";
		} else {
			//432    ERR_ERRONEUSNICKNAME
			msg_reply = g_server_hostname + " 432 * * " + " :Errorneous nickname";
		}
	} else {
		//first search if nick_name argument is already taken by someone from  inside the global hash map.
		if (!lookup_nickname_map(nick_name)) {
			if (clinfo.nickusercmd[0] == true) {
				//its change nickname request
				pthread_rwlock_wrlock(&map_rw_lock);
				g_nickname_map.erase(clinfo.nickname);
				pthread_rwlock_unlock(&map_rw_lock);
			}
			//insert nick name inside global set of nick name with sockfd as value
			insert_nickname_map(nick_name, h.sockfd);

		        //now insert this nick name inside the global server worker socket fd and client info map
			pthread_rwlock_wrlock(&map_clinfo_rw_lock);
			memcpy(clinfo.nickname, nick_name.c_str(), nick_name.size());
			clinfo.nickusercmd[0] = true;
			g_ws_clinfo_map[h.sockfd] = clinfo;
			pthread_rwlock_unlock(&map_clinfo_rw_lock);

			//fprintf(stderr, "the client hostname stored in hash map for sockfd [%d] is [%s]\n",h.sockfd, clinfo.hostname);
			msg_reply = g_server_hostname + " 001 " + nick_name + " :Welcone to the Internet Relay Network " + nick_name + "!" + nick_name + "@" + clinfo.hostname;
			//in case of succes nick command don't return any message to client
			return;
		} else {
			//433    ERR_NICKNAMEINUSE
			msg_reply = g_server_hostname + " 433 * " + nick_name + " :Nickname is already in use.";
		}
	}

	//send final reply msg to client, in case of NICK command don't send reply, the combined reply will go as part od USER command 
	reply_to_client(h.sockfd, msg_reply);
}
