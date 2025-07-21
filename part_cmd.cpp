#include "all_cmd_common.h"

void execute_cmd_part(handler_t &h, string &args_buffer)
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
	char channelname[1024];
	char beforecolon[1024];
	bool channel_create = false;
	string channel_name = "";
	vector<int> to_sfd_v;
	string str_clinfo_nickname = "";
	vector<string> nickname_channel_v;
	string nickname_list_this_channel = "";
	vector<string> channel_name_v;
	vector< vector<string> > all_given_channel_nickname;
	vector< vector<int> > all_given_channel_to_sockfd;
	vector< string >::iterator itr;
	bool nick_name_found = false;

	//fprintf(stderr, "Inside execute_cmd_join function args buffer is %s\n", args_buffer.c_str());

	//now client info from  global server worker socket fd and client info map
        pthread_rwlock_rdlock(&map_clinfo_rw_lock);
        client_info_t clinfo;
        clinfo = g_ws_clinfo_map[h.sockfd];
        pthread_rwlock_unlock(&map_clinfo_rw_lock);

	if (clinfo.nickusercmd[0] != true && clinfo.nickusercmd[1] != true) {
		//451    ERR_NOTREGISTERED
		msg_reply = ":" + g_server_hostname + "451 " + ":You have not registered";
		goto out;
	}

	//PART command-> PART #channel_name,#channel_name : priv_msg
	count = 0;
	token = strtok((char*)args.c_str(), ":\n");
	if (token != NULL) //fprintf(stderr, "The part message is %s\n", token);
	while(token != NULL) {
		memcpy(beforecolon, token, strlen(token) + 1);
		//fprintf(stderr, "The part message before colon is %s\n", beforecolon);
		count++;
		token = strtok(NULL, "\n\r");
		if (token == NULL) {
			//fprintf(stderr, "no privmsg in part command");
			break;
		}
		priv_msg = ConvertToString(token);
		//fprintf(stderr, "The priv message is [%s]\n", priv_msg.c_str());
		count++;
		break;
	}
	if (count != 2) {
		msg_reply = ":" + g_server_hostname + " 461 " + "USER :Not enough parameters";
		goto out;
	}

	count = 0;
	//#channel_name,#channel_name
	token = strtok((char*)beforecolon, " ,\n");
	if (token != NULL) //fprintf(stderr, "the token is %s\n", token);
	while (token != NULL) {
		memcpy(channelname, token, strlen(token) + 1);
		channel_name = ConvertToString(channelname);
		channel_name_v.push_back(channel_name);	
		count++;
		token = strtok(NULL, " ,\n");
		if (token == NULL) {
			//fprintf(stderr, "the token is null in part channel name\n");
			break;
		}
	}
	if (count < 1) {
		msg_reply = ":" + g_server_hostname + " 461 " + "USER :Not enough parameters";
		goto out;
	}
	for (int i = 0; i <channel_name_v.size(); i++) {
		//fprintf(stderr, "The channel name is [%s]\n", channel_name_v[i].c_str());
	}

	pthread_rwlock_wrlock(&channel_map_rw_lock);
	str_clinfo_nickname = ConvertToString(clinfo.nickname);

	for (int i = 0; i < channel_name_v.size(); i++  ) {
		if (channel_nkname_map.find(channel_name_v[i]) != channel_nkname_map.end()) {
			nickname_channel_v = channel_nkname_map[channel_name_v[i]];
			//now search for the this client nickname inside this channel and remove this nickname
			for(itr = nickname_channel_v.begin(); itr != nickname_channel_v.end(); itr++) {
				if (*itr == str_clinfo_nickname) {
					//found this client nick name inside this channel so remove it from this vector
					nickname_channel_v.erase(itr);
					nick_name_found = true;
					break;
				}
			}
			if (!nick_name_found) {
				//ERR_NOTONCHANNEL
				msg_reply = ":" + g_server_hostname + " 442 " + channel_name + " " + ":Yor're not on that channel.";
				pthread_rwlock_unlock(&channel_map_rw_lock);
				goto out;
			}
			//now remove the old entry from the channel->nickname vector map
			channel_nkname_map.erase(channel_name_v[i]);

			//now re-insert this channel name with the remaining user list 
			for (int i = 0; i < nickname_channel_v.size(); i++ ) {
				channel_nkname_map[channel_name_v[i]].push_back(nickname_channel_v[i]);
			}
			//its 2-d string vector solution ds
			all_given_channel_nickname.push_back(nickname_channel_v);
			nickname_channel_v.clear();
		} else {
			//ERR_NOSUCHCHANNEL
			msg_reply = ":" + g_server_hostname + " 403 " + channel_name + " " + ":No such channel";
			pthread_rwlock_unlock(&channel_map_rw_lock);
			goto out;
		}
	}
	pthread_rwlock_unlock(&channel_map_rw_lock);

	//make a copy of all the sockfds belongs to all the nicknames, so that the message can be sent to all the users of channel
	pthread_rwlock_rdlock(&map_rw_lock);
	nickname_channel_v.clear();
	for(int i = 0; i < all_given_channel_nickname.size(); i++) {

		//get all nickname inside a given channel
		nickname_channel_v = all_given_channel_nickname[i];
		
		for (int i = 0; i < nickname_channel_v.size(); i++) {
			//get all socket fd isnide a given channel
			int sfd = g_nickname_map[nickname_channel_v[i]];
			to_sfd_v.push_back(sfd);
		}
		all_given_channel_to_sockfd.push_back(to_sfd_v);
		to_sfd_v.clear();
		nickname_channel_v.clear();
	}
	pthread_rwlock_unlock(&map_rw_lock);
		
	//now read source client and target client informations and forward the request "from-->to" 
	pthread_rwlock_rdlock(&map_clinfo_rw_lock);
	fromclinfo = g_ws_clinfo_map[h.sockfd];
	pthread_rwlock_unlock(&map_clinfo_rw_lock);
	from_nick_name = ConvertToString(fromclinfo.nickname);
	from_user_name = ConvertToString(fromclinfo.username);
	from_hostname  = ConvertToString(fromclinfo.hostname);

	//send message to all the users, if user has joined the existing channel
	for (int i = 0; i < all_given_channel_to_sockfd.size(); i++) {
		to_sfd_v = all_given_channel_to_sockfd[i];
		for (int i = 0; i < to_sfd_v.size(); i++ ) {
			//fprintf(stderr, "the client hostname stored in hash map for sockfd [%d] is [%s]\n", to_sfd_v[i], from_hostname.c_str());
			msg_reply = ":" + from_nick_name + "!" + from_user_name + "@" + from_hostname + " PART " + channel_name + " :" + priv_msg;
			usleep(10000);
			reply_to_client(to_sfd_v[i], msg_reply);
		}
	}

	return;

out:
	reply_to_client(h.sockfd, msg_reply);
}
