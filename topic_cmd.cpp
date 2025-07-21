#include "all_cmd_common.h"

void execute_cmd_topic(handler_t &h, string &args_buffer)
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
	char topicname[1024];
	bool channel_create = false;
	string channel_name = "";
	vector<int> to_sfd_v;
	string str_clinfo_nickname = "";
	vector<string> nickname_channel_v;
	string nickname_list_this_channel = "";
	bool notopic_input = false;
	bool found_nick = false;
	bool unset_topic = false;
	string topic_name = "";
	bool foundcolon = false;
	char beforecolon[1024];

	//cmd--> TOPIC <channel> [topic]
	//set topic --> reply --> nickname!user@hostname TOPIC <channel> :topic, this message should go to all user of this channel

	//fprintf(stderr, "Inside execute_cmd_topic function args buffer is %s\n", args_buffer.c_str());

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

	//TOPIC command-> TOPIC <#channel_name> :[topicname]
	count = 0;
	if (strstr((char*)args.c_str(), ":") != NULL) {
		foundcolon = true;	
	}

	if (!foundcolon) {
		token = strtok((char*)args.c_str(), " \n\r");
		while(token != NULL) {
			memcpy(channelname, token, strlen(token) + 1);
			count++;
			notopic_input = true;
			//fprintf(stderr, "The channel name is [%s]\n", channelname);
			token = strtok(NULL, " \n\r");
			if (token == NULL) {
				break;
			}
			count++;
			break;
		}
	}
	if (!foundcolon && count != 1) {
		msg_reply = ":" + g_server_hostname + " 461 " + "USER :Not enough parameters";
		goto out;
	}

	count = 0;
	if (foundcolon) {
		token = strtok((char*)args.c_str(), ":\n");
		if (token != NULL) //fprintf(stderr, "the token is %s\n", token);
		while (token != NULL) {
			memcpy(beforecolon, token, strlen(token) + 1);
			count++;
			token = strtok(NULL, "\n\r");
			if (token == NULL) {
				//fprintf(stderr, "the token is null unset topic\n");
				unset_topic = true;
				break;
			}
			//fprintf(stderr, "the string is %s\n", token);
			memcpy(topicname, token, strlen(token) + 1);
			count++;
			break;
		}
		if (count == 0) {
			msg_reply = ":" + g_server_hostname + " 461 " + "USER :Not enough parameters";
			goto out;
		}
		count = 0;
		token = strtok(beforecolon, " \n\r");
		memcpy(channelname, token, strlen(token) + 1);
	}

	//fprintf(stderr, "The channelname [%s], topicname [%s]\n", channelname, topicname);
	channel_name = ConvertToString(channelname);
	topic_name = ConvertToString(topicname);

	channel_name = ConvertToString(channelname);

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

	if (notopic_input) {
		pthread_rwlock_rdlock(&channel_topic_rw_lock);
		if (channel_topic_map.find(channel_name) != channel_topic_map.end()) {
			//found so display the topic
			//332    RPL_TOPIC
			//"<channel> :<topic>"
			msg_reply = ":" + g_server_hostname + " 332 " + str_clinfo_nickname + " " + channel_name + " :" + channel_topic_map[channel_name];
		} else {
			//no topic yet to set on this channel
			//331    RPL_NOTOPIC
			//"<channel> :No topic is set"
			msg_reply = ":" + g_server_hostname + " 331 " + str_clinfo_nickname + " " + channel_name + " " + ":No topic is set.";
		}
		pthread_rwlock_unlock(&channel_topic_rw_lock);
		goto out;
	} 
	// if you have topic input, then set it or change it and send reply to all the users on this channel
	//now read source client and target client informations and forward the request "from-->to" 
	pthread_rwlock_rdlock(&map_clinfo_rw_lock);
	fromclinfo = g_ws_clinfo_map[h.sockfd];
	pthread_rwlock_unlock(&map_clinfo_rw_lock);
	from_nick_name = ConvertToString(fromclinfo.nickname);
	from_user_name = ConvertToString(fromclinfo.username);
	from_hostname  = ConvertToString(fromclinfo.hostname);

	if (unset_topic) {
		pthread_rwlock_wrlock(&channel_topic_rw_lock);
		if (channel_topic_map.find(channel_name) != channel_topic_map.end()) {
			channel_topic_map.erase(channel_name);
			//found so remove the topic from the channel-topic map and send reply to all the users of the channel 
			for (int i = 0; i < to_sfd_v.size(); i++ ) {
				//fprintf(stderr, "for unset the client hostname stored in hash map for sockfd [%d] is [%s]\n", to_sfd_v[i], from_hostname.c_str());
				msg_reply = ":" + from_nick_name + "!" + from_user_name + "@" + from_hostname + " TOPIC " + channel_name + " :";
				usleep(10000);
				reply_to_client(to_sfd_v[i], msg_reply);
			}
			pthread_rwlock_unlock(&channel_topic_rw_lock);
			return;
		} else {
			//no topic set on this channel yet.
			//331    RPL_NOTOPIC
			//"<channel> :No topic is set"
			msg_reply = ":" + g_server_hostname + " 331 " + str_clinfo_nickname + " " + channel_name + " " + ":No topic is set.";
		}
		pthread_rwlock_unlock(&channel_topic_rw_lock);
		goto out;
	}

	pthread_rwlock_wrlock(&channel_topic_rw_lock);
	channel_topic_map[channel_name] = topic_name;
	pthread_rwlock_unlock(&channel_topic_rw_lock);

	//send message to all the users of this channel about this new topic or changed topic  with username who has created the topic
	for (int i = 0; i < to_sfd_v.size(); i++ ) {
		//fprintf(stderr, "the client hostname stored in hash map for sockfd [%d] is [%s]\n", to_sfd_v[i], from_hostname.c_str());
		msg_reply = ":" + from_nick_name + "!" + from_user_name + "@" + from_hostname + " TOPIC " + channel_name + " :" + topic_name;
		usleep(10000);
		reply_to_client(to_sfd_v[i], msg_reply);
	}

	return;

out:
	reply_to_client(h.sockfd, msg_reply);
}
