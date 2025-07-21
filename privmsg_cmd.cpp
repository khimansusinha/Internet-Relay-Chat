#include "all_cmd_common.h"

void execute_cmd_privmsg(handler_t &h, string &args_buffer)
{
	char *token = NULL;
	string args(args_buffer);
	int count = 0;
	string from_user_name = "";
	string from_nick_name = "";
	string from_hostname = "";
	string to_nick_name = "";
	string priv_msg = "";
	string msg_reply = "";
	int to_sfd = 0;
	client_info_t fromclinfo;
	char beforecolonstr[1024];
	bool channel_name_privmsg = false;
	string channel_name = "";
	string str_clinfo_nickname = "";
	vector<string> nickname_channel_v;
	bool found_nick = false;
	vector<int> to_sfd_v;

	//fprintf(stderr, "Inside execute_cmd_privmsg function args buffer is %s\n", args_buffer.c_str());

	//now clinet info from  global server worker socket fd and client info map
        pthread_rwlock_rdlock(&map_clinfo_rw_lock);
        client_info_t clinfo;
        clinfo = g_ws_clinfo_map[h.sockfd];
        pthread_rwlock_unlock(&map_clinfo_rw_lock);

	if (clinfo.nickusercmd[0] != true && clinfo.nickusercmd[1] != true) {
		//451    ERR_NOTREGISTERED
		msg_reply = ":" + g_server_hostname + "451 " + ":You have not registered";
		goto out;
	}

	//privmsg command-> PRIVMSG rory :Hey rory...
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
		priv_msg = ConvertToString(token);
		//fprintf(stderr, "the second token real name is %s\n", priv_msg.c_str());
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
		to_nick_name = ConvertToString(token);
		if (to_nick_name[0] == '#') {
			channel_name_privmsg = true;
			channel_name = to_nick_name;
			//fprintf(stderr, "The channel name is %s\n", channel_name.c_str());
		}
		//fprintf(stderr, "The user name given is: %s\n", to_nick_name.c_str());
		count++;

		token = strtok(NULL, " \n\r");
		if (token == NULL) {
			//fprintf(stderr, "error in usermode token\n");
			break;
		}
		//fprintf(stderr, "the user mode is: %s\n", token);
		count++;

		break;
	}
	
	if (count != 1) {
		//461    ERR_NEEDMOREPARAMS
		msg_reply = ":" + g_server_hostname + " 461 " + "PRIVMSG :Not enough parameters";
		goto out;
	}

	//now read source client and target client informations and forward the request "from-->to" 
	pthread_rwlock_rdlock(&map_clinfo_rw_lock);
	fromclinfo = g_ws_clinfo_map[h.sockfd];
	pthread_rwlock_unlock(&map_clinfo_rw_lock);

	from_nick_name = ConvertToString(fromclinfo.nickname);
	from_user_name = ConvertToString(fromclinfo.username);
	from_hostname  = ConvertToString(fromclinfo.hostname);


	if (channel_name_privmsg) {
		//this privmsg for all users of the channel
		pthread_rwlock_wrlock(&channel_map_rw_lock);
		str_clinfo_nickname = ConvertToString(clinfo.nickname);
		if (channel_nkname_map.find(channel_name) == channel_nkname_map.end()) {
			//403    ERR_NOSUCHCHANNEL
			//"<channel name> :No such channel"
			msg_reply = ":" + g_server_hostname + " 403 " + channel_name + " " + ":No such channel";
			pthread_rwlock_unlock(&channel_map_rw_lock);
			goto out;
		}
		//found channel now check if the user is part of this channel
		//already channel, so server will relay join message to all the existing user of this channel.
		nickname_channel_v = channel_nkname_map[channel_name];
		pthread_rwlock_unlock(&channel_map_rw_lock);

		//make a copy of all the sockfds belongs to all the nicknames, so that the message can be sent to all the users of channel
		pthread_rwlock_rdlock(&map_rw_lock);
		for (int i = 0; i < nickname_channel_v.size(); i++) {
			if (nickname_channel_v[i] == str_clinfo_nickname) {
				found_nick = true;
			}
			to_sfd_v.push_back(g_nickname_map[nickname_channel_v[i]]);
		}
		pthread_rwlock_unlock(&map_rw_lock);
		if (!found_nick) {
			//442    ERR_NOTONCHANNEL
			//<channel> :You're not on that channel"
			msg_reply = ":" + g_server_hostname + " 442 " + channel_name + " " + ":Yor're not on that channel.";
			goto out;
		}

		//send privmsg to all users of this channel with username who has sent this privmsg 
		for (int i = 0; i < to_sfd_v.size(); i++ ) {
			//fprintf(stderr, "the client hostname stored in hash map for sockfd [%d] is [%s]\n", to_sfd_v[i], from_hostname.c_str());
			msg_reply = ":" + from_nick_name + "!" + from_user_name + "@" + from_hostname + " PRIVMSG " + channel_name + " :" + priv_msg;
			usleep(10000);
			// don't send to himself, but send privmsg to all others in the channel
			if (to_sfd_v[i] != h.sockfd) {
				reply_to_client(to_sfd_v[i], msg_reply);
			}
		}
		return;
	}

	//now check if to_nick_name is registered or not
	pthread_rwlock_rdlock(&map_rw_lock);
	if(g_nickname_map.find(to_nick_name) == g_nickname_map.end()) {
		//401    ERR_NOSUCHNICK
		//"<nickname> :No such nick/channel"
		msg_reply = ":" + g_server_hostname + " 401 " + to_nick_name + ":" + "No such nickname";
		pthread_rwlock_unlock(&map_rw_lock);
		goto out;
	}
	//now find server worker socket fd corresponding to this to_nick_name from the global map
	to_sfd = g_nickname_map[to_nick_name];
	//fprintf(stderr, "The to sock fd for nick [%s] is [%d]\n", to_nick_name.c_str(), to_sfd);
	pthread_rwlock_unlock(&map_rw_lock);

	//fprintf(stderr, "the client hostname stored in hash map for sockfd [%d] is [%s]\n",h.sockfd, clinfo.hostname);
	msg_reply = ":" + from_nick_name + "!" + from_user_name + "@" + from_hostname + " PRIVMSG " + to_nick_name + " :" + priv_msg;


out:
	//send final reply msg to client 
	if (to_sfd ) {
		reply_to_client(to_sfd, msg_reply);
	} else {
		reply_to_client(h.sockfd, msg_reply);
	}
}
