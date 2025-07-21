#include "all_cmd_common.h"

void parseCommand(handler_t &h, string &cmd_buffer, string &args_cmd)
{
	string client_msg(h.msg);
	string buffer;

	//fprintf(stderr, "in parseCommand the message is %s\n", client_msg.c_str());

	//get the command which is first word of the message and remaining are args
	char *token = NULL;
	token = strtok((char*)client_msg.c_str(), " \n\r");
	//fprintf(stderr, "in parseCommand the token is %s\n", token);
	while(token != NULL) {
		cmd_buffer = ConvertToString(token);
		//fprintf(stderr, "the cmd is %s\n", cmd_buffer.c_str());
		token = strtok(NULL, "\n\r");//second token is based on \n pr \r not space, i.e. take all words after command as args of cmd
		if (token == NULL) {
			break;
		}
		args_cmd = ConvertToString(token);
		//fprintf(stderr, "the args string is %s\n", args_cmd.c_str());
		break;
	}
}

void reply_to_client(int to_sockfd, string &reply)
{
	write(to_sockfd, reply.c_str(), strlen(reply.c_str()) + 1);
}

void parseAndExecuteCmd(handler_t &h)
{
	string cmd_buffer = "";
	string args_buffer = "";

	//fprintf(stderr, "Inside parseAndExcute function\n");	
	//parse the message sent by the client

	if (h.msg != "QUIT") { 
		parseCommand(h, cmd_buffer, args_buffer);
	} else {
		cmd_buffer = "QUIT";
	}

	if (cmd_buffer == "NICK") {
		execute_cmd_nick(h, args_buffer);

	} else if (cmd_buffer == "USER") {
		execute_cmd_user(h, args_buffer);

	} else if (cmd_buffer == "TIME") {
		execute_cmd_time(h, args_buffer);

	} else if (cmd_buffer == "JOIN") {
		execute_cmd_join(h, args_buffer);

	} else if (cmd_buffer == "TOPIC") {
		execute_cmd_topic(h, args_buffer);

	} else if (cmd_buffer == "PRIVMSG") {
		execute_cmd_privmsg(h, args_buffer);

	} else if (cmd_buffer == "NAMES") {
		execute_cmd_names(h, args_buffer);

	} else if (cmd_buffer == "PART") {
		execute_cmd_part(h, args_buffer);

	} else {
		//command not suppported
		//421 ERR_UNKNOWNCOMMAND
		string msg_reply = ":" + g_server_hostname + " 421 " + cmd_buffer + " :Unkonwn command"; 
		reply_to_client(h.sockfd, msg_reply);
	}
}

void clean_sockentry(int sockfd)
{

        //now clinet info from  global server worker socket fd and client info map
        pthread_rwlock_rdlock(&map_clinfo_rw_lock);
        client_info_t clinfo;
        clinfo = g_ws_clinfo_map[sockfd];
        pthread_rwlock_unlock(&map_clinfo_rw_lock);


        pthread_rwlock_wrlock(&channel_map_rw_lock);
        string str_clinfo_nickname = ConvertToString(clinfo.nickname);
	vector<string> nickname_channel_v;
	unordered_map< string, vector<string> >::iterator itr;
	vector<string>::iterator itr1;
	string channel_name;
	bool nick_name_found = false;
	vector<string> channel_name_v;

	for (itr = channel_nkname_map.begin(); itr != channel_nkname_map.end(); itr++) {
		channel_name_v.push_back(itr->first);
	}
	for (int i = 0; i < channel_name_v.size(); i++ ) {
		if (channel_nkname_map.find(channel_name_v[i]) != channel_nkname_map.end()) {
			nickname_channel_v = channel_nkname_map[channel_name_v[i]];
			//now search for the this client nickname inside this channel and remove this nickname
			for(itr1 = nickname_channel_v.begin(); itr1 != nickname_channel_v.end(); itr1++) {
				if (*itr1 == str_clinfo_nickname) {
					//found this client nick name inside this channel so remove it from this vector
					nickname_channel_v.erase(itr1);
					nick_name_found = true;
					break;
				}
			}
			if (nick_name_found) {
				//now remove the old entry from the channel->nickname vector map
				channel_nkname_map.erase(channel_name);

				//now re-insert this channel name with the remaining user list
				for (int i = 0; i < nickname_channel_v.size(); i++ ) {
					channel_nkname_map[channel_name].push_back(nickname_channel_v[i]);
				}
				nickname_channel_v.clear();
			}
		}
	}
        pthread_rwlock_unlock(&channel_map_rw_lock);

	pthread_rwlock_wrlock(&map_clinfo_rw_lock);
	if (g_ws_clinfo_map.find(sockfd) != g_ws_clinfo_map.end()) {
		//fprintf(stderr, "Removing sock fd entry from map\n");
		clinfo = g_ws_clinfo_map[sockfd];
		g_ws_clinfo_map.erase(sockfd);
	}
	pthread_rwlock_unlock(&map_clinfo_rw_lock);

	pthread_rwlock_wrlock(&map_rw_lock);
	//now also remove it from nickname set
	if(g_nickname_map.find(clinfo.nickname) != g_nickname_map.end()) {
		//fprintf(stderr, "Removing nickname entry from map\n");
		g_nickname_map.erase(clinfo.nickname);
	}
	pthread_rwlock_unlock(&map_rw_lock);
}


void* worker_loop(void *arg)
{
	//fprintf(stderr, "Inside worker loop\n");
	while(1) {

		pthread_mutex_lock(&m);

		while (gworklist.empty()) {
			pthread_cond_wait(&cv, &m);
		}

		if (g_stop) {
			pthread_mutex_unlock(&m);
			break;
		}

		handler_t h = gworklist.front();
		gworklist.pop_front();
		pthread_mutex_unlock(&m);

		//exit yourslef if you pick up the kill yourself message object from the server
		if (h.flag) {

			//fprintf(stderr, "exiting one thread for socket fd %d\n", h.sockfd);
			//first clean himself from different hash tables, note this server worker socket fd we received during
			//accept() call and we have maintain its corrsponding client info inide the hash, and since its corresponding
			//clinet got terminated so we have recvied the message on its corresponding worker socket, so we have delete
			//this server worker socket fd and its client entry also from all places. Note we have aready removed this sock fd
			//entry from the sockfd list in server main thread where select() has called when it gets client termination msg.
			//now clinet info from  global server worker socket fd and client info map
			pthread_rwlock_rdlock(&map_clinfo_rw_lock);
			client_info_t clinfo;
			clinfo = g_ws_clinfo_map[h.sockfd];
			pthread_rwlock_unlock(&map_clinfo_rw_lock);
			fprintf(stderr, "received QUIT command: exiting one thread for socket fd %d\n", h.sockfd);
			clean_sockentry(h.sockfd);
			string quit_msg = "QUIT";
			execute_cmd_quit(h, quit_msg, clinfo);
			close(h.sockfd);
			break;
		}

		//now process the handler object here, by the worker thread and send the response to the client from here
		parseAndExecuteCmd(h);

		//send resonse to the client
		//note, you write on the server socket fd, and this local server socket fd is associated with the client socket fd
		//this association is done during accept or connection establishment, this association is maintained by the kernel
		//so, we don't need to worry, we can simply write to the associated local server sockfd and its source associated
		//client socketfd will automatically get the answer, this is done by the kernel.
		//think, you have a black board at both the ends, and each end does read and write to its own black board,
		//the response of write on the local board will be send to its associated client board by the kernel and the client
		//will read the response on its own end black borad.

		//write(h.sockfd, res.c_str(), strlen(res.c_str()));
	}

	return NULL;
}
