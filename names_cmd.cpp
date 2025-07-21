#include "all_cmd_common.h"


void execute_cmd_names(handler_t &h, string &args_buffer)
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
	bool list_all_channel_nickname = false;
	string ch_name = "";
	string all_ch_nk_name = "";
	vector<string> nk_name_v;
	unordered_map<string, vector<string> >::iterator itr;
	unordered_map<string, int>::iterator itr1;

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

	//NAMES command-> NAMES [#channel_name] 
	count = 0;
	token = strtok((char*)args.c_str(), " \n");
	if (token == NULL) {
		//fprintf(stderr, "NO token\n");
		list_all_channel_nickname = true;
	} 
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
	if (count > 1) {
		msg_reply = ":" + g_server_hostname + " 461 " + "USER :Not enough parameters";
		goto out;
	}
	//fprintf(stderr, "The channel name is [%s]\n", channelname);

	channel_name = ConvertToString(channelname);

	str_clinfo_nickname = ConvertToString(clinfo.nickname);

	if (!list_all_channel_nickname) {
		if (channel_nkname_map.find(channel_name) != channel_nkname_map.end()) {
			pthread_rwlock_rdlock(&channel_map_rw_lock);
			nickname_channel_v = channel_nkname_map[channel_name];
			pthread_rwlock_unlock(&channel_map_rw_lock);

			for (int i = 0; i < nickname_channel_v.size(); i++) {
				nickname_list_this_channel += nickname_channel_v[i] + ", ";	
			}
			msg_reply = ":" + g_server_hostname + " 353 " + from_nick_name + " " + channel_name + " " + nickname_list_this_channel;
			goto out;
		} else {
			//not found channel so create this channel, this guy will be the admin of this channel
			msg_reply = ":" + g_server_hostname + " 403 " + channel_name + " " + ":No such channel";
			goto out;
		}
	}

	//now channel is not given so list all the nick names on all the channels

	pthread_rwlock_rdlock(&channel_map_rw_lock);
	for (itr = channel_nkname_map.begin(); itr != channel_nkname_map.end(); itr++) {
		ch_name = itr->first;
		all_ch_nk_name += ch_name + " ";
		nk_name_v = itr->second;
		for (int i = 0; i < nk_name_v.size(); i++) {
			all_ch_nk_name += nk_name_v[i] + " ";
		}
	}
	pthread_rwlock_unlock(&channel_map_rw_lock);
	//when no channel is created at all, so list down all the nick names globally which are present
	if (channel_nkname_map.size() == 0) {
		pthread_rwlock_rdlock(&map_rw_lock);
		for (itr1 = g_nickname_map.begin(); itr1 != g_nickname_map.end(); itr1++) {
			all_ch_nk_name += itr1->first + " ";
		} 
		pthread_rwlock_unlock(&map_rw_lock);
	}

	usleep(10000);
	msg_reply = ":" + g_server_hostname + " " + all_ch_nk_name;

out:
	reply_to_client(h.sockfd, msg_reply);
}
