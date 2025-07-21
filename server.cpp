#include "server.h"
#include "worker.h"
#include "all_cmd_common.h"

int server_port;
char nick[1024];
char pass[1024];
char servers[1024];
char sock_addr[1024];

//golbal list which will be shared between main server thread as producer and all other worker thread as consumer.
//worker threads are responsible to process the client request and response.
//the main server thread is only responsible to produce the handler and append this object inside the global list

list<handler> gworklist;

//now you have global work list as critical section, so you need to handle synchronization.
//the server main threads waits on select, so we don't needed condition variable for server producer in this case.
//we needed one condition variable for all the server worker threads only to wait for if nothing to produce.
//we needed one mutex which will be used by both main server thread and all the server worker threads.

string g_server_hostname;
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
int g_stop = 0;

pthread_rwlock_t map_rw_lock;
unordered_map<string, int> g_nickname_map;

pthread_rwlock_t map_clinfo_rw_lock;
unordered_map<int, client_info_t> g_ws_clinfo_map; //worker socket fd and its corresponsing client info map

pthread_rwlock_t channel_map_rw_lock;
unordered_map<string, vector<string> > channel_nkname_map;

pthread_rwlock_t channel_topic_rw_lock;
unordered_map<string, string> channel_topic_map;

void start_server(void)
{
	int sfd;
	struct sockaddr_in address, client_address;
	int addrlen = sizeof(address);
	vector<int> sockfdlist(MAX_CLIENTS);
	int clientcount = 0;
	fd_set readfds;
	int max_fd;
	char buffer[4096];

	FD_ZERO(&readfds);

	//create socket
	if ((sfd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
		cout<<"server socket creation failed"<<endl;
		exit(1);
	}

	//fill the struct sockaddr_in to fill the struct used to bind this socket to a port
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(server_port);

	//now call the bind
	if (bind(sfd, (struct sockaddr *)&address, sizeof(address))< 0 ) {
		cout<<"failed to bind server socket on port"<<endl;
		exit(1);
	}

	//prepare socket server to listen for the connection
	if (listen(sfd, MAX_CLIENTS) < 0) {
		cout <<"server listen failed"<<endl;
		exit(1);
	}

	//Now loop and wait for new connection request
	while (1) {

		//set FD_SET each time, because after select returns, all the fds that have no associated events occur will be cleared.
		//for example you have sockfd set (1, 2, 3) and say event occurs only for 2, then after select returns you will have
		// only (0, 2, 0) in the fd set, so you need to set each time the fd set so that it will keep monitoring all the fd sets.
		//NOTE: this is very very important step, you need to reset all the sock fd inside readfds each time you return from select
		//
		//Now why we need to use select() call.
		//in select() call we use to monitor all the sockets inside the main thread, so that the async processing can be achieved
		// by the server worker thread, so the server main thread will be sct as a producer and whenever the event comes it reads
		// the data from the socket and then copy it inside an object and insert this object inside a gloabl shared list on
		// which all the worker threads are waiting on... the global list object contains the sockfd on which the question has
		//arrived and also the question, and the server worker thread processes the head of the list ater removing it from head
		//the global shared list, and computes the answer of the questions, after processing it sends the answer to the same
		//socket fd which is present inside the link list object i.e. the object which was the current thread was processing.
		// so response will be send by the worker thread directly to the client while the question will be sent by the main
		//server thread to the server worker thread by putting all the things inside and list object and inserting this object
		//inside the global shared list and the server main thread makes a wake up call to all the server worker threads.
		// SO, by doing select we are implementing async call for server main thread, so that server main thread can handle next set
		// of events coming on the socket FDs,
		// the server worker threads are sleeping inside the condition variable and the server main thread
		// producess the object and give a signal to the server worker threads.

		//while if we don't want to use select i.e. async method to process by all the server worker threads, then
		//will need to make all the server worker threads block on the readline() call on each socket.
		//i.e. each server worker thread will be closely bind with one of the socket created by the server and
		//each server worker thread keeps its read stream and write stream and block on readline() to get the data from the client
		//and when question comes from the client its corresponding waiting server worker thread always replies directly to the
		//client. i.e. each server worker thread is doing sync call of request and response from the client.

		FD_CLR(sfd, &readfds);
		FD_SET(sfd, &readfds);
		max_fd = sfd;

		//now reset all the helper socket fds
		for (int i = 0; i < clientcount; i++) {
			FD_CLR(sockfdlist[i], &readfds);
			FD_SET(sockfdlist[i], &readfds);
			if ( sockfdlist[i] > max_fd ) {
				max_fd = sockfdlist[i];
			}
		} 

		//you keep a list of sockets made by server inside an array, so that we can give it to select() to monitor all sfds
		//first argument is higher fd number in all set +1
		//select is a waiting call it waits till signal happens on any of fd present inside the read fd set.

		//cout<<"waiting on select for new connection or data request"<<endl;
		if (select(max_fd+1, &readfds, NULL, NULL, NULL) == -1) {
			cout <<"select error"<<endl;
			exit(1);
		}

		//cout<<"came out from select wait, now check its new connection or data request"<<endl;

		//select got signal for at least one of the fd mentioned inside the read fd set, so process it.
		if (FD_ISSET(sfd, &readfds)) {
			//means signal in the main socket server fd i.e. its a connect call for new client, so accept this connection
			//and store the new sockfd return by accept inside an array, so that it can be used in fututre calls to find
			//out the select signals arrived for what are the socket numbers, so that the worker thread can directly use
			//this socket number to process.
			//the readfds is a bit map of the each value present inside the array of socket fds 

			//cout<<"Got new connection reqeuest from the client"<<endl;

			//store the new socket which maps to a client inside an socket array
			sockfdlist[clientcount] = accept(sfd, (struct sockaddr *)&client_address, (socklen_t *)&addrlen);

			int port;
			char ipstr[1024];
			char client_host[1024];
			char service[20];
			struct sockaddr_storage addr;
			socklen_t len;
			bool found_hostname = false;

			if (getnameinfo((struct sockaddr*)&client_address, addrlen, client_host, sizeof(client_host), service, sizeof(service), 0) == 0) {
				//cout <<"The client host name is: "<<client_host<<endl;
				found_hostname = true;
			} else {
				//if not able to get the client host name get client IP address
				if(getpeername(sockfdlist[clientcount], (struct sockaddr*)&addr, &len) == 0) {
					if (addr.ss_family == AF_INET) {
						struct sockaddr_in *s = (struct sockaddr_in*)&addr;
						port = ntohs(s->sin_port);
						inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof(ipstr));
						//cout <<"The ip address: "<<ipstr<<"and port is: "<<port<<endl;
					}
				}
			}

			//now allocate a client_info_t object to cache a mapping of worker socket fd and its client info
			client_info_t clinfo;
			if (found_hostname) {
				memcpy(clinfo.hostname, client_host, sizeof(client_host));
				//cout<<"The copied client hostname is: "<<clinfo.hostname<<endl;
			} else {
				memcpy(clinfo.hostname, ipstr, sizeof(ipstr));
				//cout<<"The copied client IP address is: "<<clinfo.hostname<<endl;
			}

			//now insert this into worker socket fd and client global map which will be used by worker threads later
			pthread_rwlock_wrlock(&map_clinfo_rw_lock);

			//do make_pair so that the copy of the clinfo local variable can be stored inside the global hash map
			//clinfo is a local variable so you to make its copy inside the gloable hash map i.e. inside data section.
			g_ws_clinfo_map.insert(make_pair(sockfdlist[clientcount], clinfo));

			pthread_rwlock_unlock(&map_clinfo_rw_lock);

			//cout<<"The client local host name inside the map is: "<<g_ws_clinfo_map[sockfdlist[clientcount]].hostname<<endl;

			clientcount++;
			pthread_t tid;
			//now create a server worker thread for each new connection.
			//Note, here worker_loop a=has argument NULL, that means, the worker threads is not tightly coupled
			//to serve a particular socket fd, instead we have decopuled all the threads with the sock fd.
			//by doing this any thread can serve any sock fd in its one iteration and in its next iteration it
			//may server differnet socket fd.
			//We can achieve it with the halp of the server, it gets the client data and puts the client sockfd and its data
			//inside an object and insert this object inside a linked list, each client thread picks up one object in
			//its one thread iteration and serve one of the object i.e. one client socket fd and its data inside one handler
			//object and this handler object inside the global list inserted by the server main thread.
			//The benefit of doing this: If a client has more number of requests and another client has very less number of
			//requests, then in that case the second thread sits ideal, if we would had implemented it by using,
			//a tight mapping of one thread with one socket fd. But, after decoupling the threads with the socket fds,
			//now all the threads may be involve to serve the requests from the commonm pool or a set of requests
			//from the different client, so the threads will not sit ideal and instead it serves the other clients which
			//has more number of reauests.
			//Drawbacks: for each client we are creating a worker thread, so if there qre 1K clients then we may end up creating
			//1K threads, instead we could have used thread pool, where number of threads in thread pool will be equal to the
			//number of cores in the CPU.
			if(pthread_create(&tid, NULL, worker_loop, NULL) != 0) {
				cout<<"Error in creating a pthread"<<endl;
				exit(1);
			}
		} else {
			//cout<<"Got data request from the client"<<endl;
			//data request from the client.
			for (int i = 0; i < clientcount; i++) {
				if (FD_ISSET(sockfdlist[i], &readfds)) {
					//read the data request for this socket fd which is sent by its client
					memset(buffer, 0, sizeof(buffer));
					bool clientexited = false;
					int removed_sockfd;
					int rv;

					if ( ((rv = read(sockfdlist[i], buffer, sizeof(buffer))) == 0) ||
						(strcmp(buffer, "QUIT") == 0)) {
						//when client terminates using ctrl-c or QUIT,
						//its corresponding sock fd gets an event in server so select returns,
						//now when main thread tries to read it, read fails (<=0) that means client exited.
						//we don't need to exit the server until all client exited.
						//we need to remove this fd from the sockfdlist and also clear the redfds for this fd
						//also decrease clientcount, if client coutn reaches zero then exit the server.
						//when one client terminates we also need to exit one of the thread, because we have
						//number of worker thread in server is euqal to the number of clients present.
						//to do it, this main thread can set a flag inside the handler, and whatever thread
						//picks up this exit flag handler object will exit itself, in that case only one thread
						//exits who got the exit flag handler object, rest all will be keep running.
						//cout <<"client terminated for server sock fd: "<<sockfdlist[i]<<endl;
						//clear this fd flag inside the sockfd array, we don't want further event on this socket
						//FD_CLR(sockfdlist[i], &readfds);
						vector<int>::iterator itr;
						for (itr = sockfdlist.begin(); itr!=sockfdlist.end(); itr++) {
							if (*itr == sockfdlist[i]) {
								//remove this fd from the sockfd list
								//cout<<"Removing sockfd [%d]"<< sockfdlist[i]<<endl;
								removed_sockfd = sockfdlist[i];
								FD_CLR(sockfdlist[i], &readfds);
								sockfdlist.erase(itr);
								clientexited = true;
								clientcount--;
								break;
							}
						}
						//continue;
						//exit(1);
					} else if( rv < 0 ) {
						//cout<< "Server failed to read client request data for socket fd: "<<sockfdlist[i]<<endl;
						continue;
					}

					//printf("The received message from client is %s\n", buffer);

					//allocate an handler object and add it to the global list
					handler_t h;

					h.flag = 0;
					if (!clientexited) {
						h.sockfd = sockfdlist[i];
						h.msg = ConvertToString(buffer);

					} else {
						h.sockfd = removed_sockfd;
						//order one of the thread to terminate himself, whoever thread picks up this handle object
						h.flag = 1;
						h.msg = "QUIT";
					}

 					//now take a mutex here, add the buffer inside the global list and signal the worker threads
					pthread_mutex_lock(&m);

					gworklist.push_back(h);
					pthread_cond_signal(&cv);

					pthread_mutex_unlock(&m);

				}
			}
		}
	}
}

void read_conf_file(char *file_name)
{
	FILE *fp;
	char buffer[MAXLINE];
	char *token;
	int ret = 0;

	fp = fopen(file_name, "r");
	if(!fp) {
		cout<<"Error in opening input conf file: "<<file_name<<endl;
		ret = -1;
		goto out;
	}
	fscanf(fp, "%s", buffer);
	token = strtok(buffer, "=\n");
	if (strcmp(token, "NICK")) {
		cout<<"Invalid string in server.conf file, it should be NICK=<server_nickname>"<<endl;
		ret = -1;
		goto out;
	}
	while(token != NULL) {
		token = strtok(NULL, "=\n");
		if (token == NULL) break;
		memcpy(nick, token, strlen(token));
		break;
	}
	memset(buffer, 0, sizeof(buffer));
	fscanf(fp, "%s", buffer);
	token = strtok(buffer, "=\n");
	if (strcmp(token, "PASS")) {
		cout<<"Invalid string in server.conf file, it should be PASS="<<endl;
		ret = -1;
		goto out;
	}
	while(token != NULL) {
		token = strtok(NULL, "=\n");
		if (token == NULL) break;
		memcpy(pass, token, strlen(token));
		break;
	}
	memset(buffer, 0, sizeof(buffer));
	fscanf(fp, "%s", buffer);
	token = strtok(buffer, "=\n");
	if (strcmp(token, "PORT")) {
		cout<<"Invalid string in server.conf file, it should be SERVER_PORT=<server-port-number>"<<endl;
		ret = -1;
		goto out;
	}
	while(token != NULL) {
		token = strtok(NULL, "=\n");
		if (token == NULL) break;
		server_port = atoi(token);
		//fprintf(stderr, "The port number is [%d]\n", server_port);
		break;
	}
	memset(buffer, 0, sizeof(buffer));
	fscanf(fp, "%s", buffer);
	token = strtok(buffer, "=\n");
	if (strcmp(token, "SERVERS")) {
		cout<<"Invalid string in server.conf file, it should be SERVERS="<<endl;
		ret = -1;
		goto out;
	}
	while(token != NULL) {
		token = strtok(NULL, "=\n");
		if (token == NULL) break;
		memcpy(servers, token, strlen(token));
		break;
	}
	memset(buffer, 0, sizeof(buffer));
	fscanf(fp, "%s", buffer);
	token = strtok(buffer, "=\n");
	if (strcmp(token, "SOCK_ADDR")) {
		cout<<"Invalid string in server.conf file, it should be SOCK_ADDR="<<endl;
		ret = -1;
		goto out;
	}
	while(token != NULL) {
		token = strtok(NULL, "=\n");
		if (token == NULL) break;
		memcpy(servers, token, strlen(token));
		break;
	}

out:
	fclose(fp);
	if (ret == -1) {
		exit(1);
	}

	return;
}

int main(int argc, char *argv[]) {

	pthread_rwlock_init(&map_rw_lock, NULL);
	pthread_rwlock_init(&map_clinfo_rw_lock, NULL);

	char server_hostname[128];
	gethostname(server_hostname, sizeof(server_hostname));
	g_server_hostname = ConvertToString(server_hostname); 
	//cout<<"The server hostname is "<<g_server_hostname<<endl;

	if (argc != 2) {
		cout <<"Incorrect number of arguments"<<endl;
		exit(1);
	}

	read_conf_file(argv[1]);
	start_server();

	pthread_rwlock_destroy(&map_rw_lock);
	pthread_rwlock_destroy(&map_clinfo_rw_lock);
	pthread_rwlock_destroy(&channel_map_rw_lock);
	pthread_rwlock_destroy(&channel_topic_rw_lock);

	return 0;
}
