#include "all_cmd_common.h"

void execute_cmd_join(handler_t &h, string &args_buffer)
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
	bool channel_create = false;
	string channel_name = "";
	vector<int> to_sfd_v;
	string str_clinfo_nickname = "";
	vector<string> nickname_channel_v;
	string nickname_list_this_channel = "";
	string map_topic_name = "";
	vector<string> channel_nkname_v;
	unordered_map<string, vector<string> >::iterator itr;

	//fprintf(stderr, "Inside execute_cmd_join function args buffer is %s\n", args_buffer.c_str());

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

	//JOIN command-> JOIN #channel_name 
	count = 0;
	token = strtok((char*)args.c_str(), " \n");
	if (token != NULL) //fprintf(stderr, "the token is %s\n", token);

	while (token != NULL) {
		memcpy(channelname, token, strlen(token) + 1);
		count++;
		token = strtok(NULL, " \n\r");
		if (token == NULL) {
			//fprintf(stderr, "the token is null\n");
			break;
		}
		count++;
		break;
	}
	if (count != 1) {
		msg_reply = ":" + g_server_hostname + " 461 " + "USER :Not enough parameters";
		goto out;
	}
	//fprintf(stderr, "The channel name is [%s]\n", channelname);

	channel_name = ConvertToString(channelname);
	
	if (channelname[0] != '#') {
		//ERR_BADCHANNELKEY
		msg_reply = msg_reply = ":" + g_server_hostname + " 475 " + channel_name + " :Cannot join channel"; 
		goto out;
	}
	else if ( strlen(channelname) > 50 ) {
		//ERR_BADCHANNELKEY
		msg_reply = ":" + g_server_hostname + " 475 " + channel_name + " :Cannot join channel"; 
		goto out;
	}

	pthread_rwlock_wrlock(&channel_map_rw_lock);
	str_clinfo_nickname = ConvertToString(clinfo.nickname);

	if (channel_nkname_map.find(channel_name) != channel_nkname_map.end()) {
		//found channel so join this channel
		//before joinin channel check if the nickname is already on channel
		for (itr = channel_nkname_map.begin(); itr != channel_nkname_map.end(); itr++) {
			channel_nkname_v = itr->second;
			for (int i = 0; i < channel_nkname_v.size(); i++) {
				if(channel_nkname_v[i] == str_clinfo_nickname) {
					//443 ERR_USERONCHANNEL
					msg_reply = ":" + g_server_hostname + " 443 " + str_clinfo_nickname + " " + channel_name + " " + ":is already on channel";
					pthread_rwlock_unlock(&channel_map_rw_lock);
					goto out;
				}
			}
		}
		//already not in channel so insert the user inside the channel
		channel_nkname_map[channel_name].push_back(str_clinfo_nickname);

	} else {
		//not found channel so create this channel, this guy will be the admin of this channel
		channel_nkname_map[channel_name].push_back(str_clinfo_nickname);
		//fprintf(stderr, "created channel name %s\n", channel_name.c_str());
		channel_create = true;
	}
	nickname_channel_v = channel_nkname_map[channel_name];
	pthread_rwlock_unlock(&channel_map_rw_lock);

	if (!channel_create) {

		//already channel, so server will relay join message to all the existing user of this channel.
		//make a copy of all the sockfds belongs to all the nicknames, so that the message can be sent to all the users of channel
		pthread_rwlock_rdlock(&map_rw_lock);
		//nickname_channel_v = channel_nkname_map[channel_name];
		for (int i = 0; i < nickname_channel_v.size(); i++) {
			int sfd = g_nickname_map[nickname_channel_v[i]];
			to_sfd_v.push_back(sfd);
		}
		pthread_rwlock_unlock(&map_rw_lock);
	}
		
	//now read source client and target client informations and forward the request "from-->to" 
	pthread_rwlock_rdlock(&map_clinfo_rw_lock);
	fromclinfo = g_ws_clinfo_map[h.sockfd];
	pthread_rwlock_unlock(&map_clinfo_rw_lock);
	from_nick_name = ConvertToString(fromclinfo.nickname);
	from_user_name = ConvertToString(fromclinfo.username);
	from_hostname  = ConvertToString(fromclinfo.hostname);
	if (channel_create) {
		//send message only to the user who has created this channel
		msg_reply = ":" + from_nick_name + "!" + from_user_name + "@" + from_hostname + " JOIN " + channel_name;
		reply_to_client(h.sockfd, msg_reply);

		usleep(10000);
		//331    RPL_NOTOPIC
		//"<channel> :No topic is set"
		msg_reply = ":" + g_server_hostname + " 331 " + from_nick_name + " " + channel_name + ":No topic is set";
		reply_to_client(h.sockfd, msg_reply);

		usleep(10000);
		//366 RPL_ENDOFNAMES
		//"<channel> :End of NAMES list"
		msg_reply = ":" + g_server_hostname + " 366 " + from_nick_name + " " + channel_name + ":End of NAMES list";
		reply_to_client(h.sockfd, msg_reply);
		return;
	}
	//send message to all the users, if user has joined the existing channel
	for (int i = 0; i < to_sfd_v.size(); i++ ) {
		//fprintf(stderr, "the client hostname stored in hash map for sockfd [%d] is [%s]\n", to_sfd_v[i], from_hostname.c_str());
		msg_reply = ":" + from_nick_name + "!" + from_user_name + "@" + from_hostname + " JOIN " + channel_name;
		reply_to_client(to_sfd_v[i], msg_reply);
	}

	//fprintf(stderr, "the sockfd is %d\n", h.sockfd);
	//now send 3 more replies only to new user who has joined this existing channel
	//331
	usleep(10000);
	pthread_rwlock_rdlock(&channel_topic_rw_lock);
	if (channel_topic_map.find(channel_name) != channel_topic_map.end()) {
		map_topic_name = channel_topic_map[channel_name];
		msg_reply = ":" + g_server_hostname + " TOPIC " + from_nick_name + " " + channel_name + ":" + map_topic_name;
	} else {
		msg_reply = ":" + g_server_hostname + " 331 " + from_nick_name + " " + channel_name + ":No topic is set";
	}
	pthread_rwlock_unlock(&channel_topic_rw_lock);
	reply_to_client(h.sockfd, msg_reply);

	usleep(10000);
	//353 RPL_NAMERPLY
	for (int i = 0; i < nickname_channel_v.size(); i++ ) {
		nickname_list_this_channel = nickname_list_this_channel + nickname_channel_v[i] + " ";
	}
	msg_reply = ":" + g_server_hostname + " 353 " + from_nick_name + " " + channel_name + " " + nickname_list_this_channel; 
	reply_to_client(h.sockfd, msg_reply);

	usleep(10000);
	//366 RPL_ENDOFNAMES
	msg_reply = ":" + g_server_hostname + " 366 " + from_nick_name + " " + channel_name + ":End of NAMES list";
	reply_to_client(h.sockfd, msg_reply);

	return;

out:
	reply_to_client(h.sockfd, msg_reply);
}
