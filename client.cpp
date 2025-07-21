#include<iostream>
#include<cstdlib>
#include<pthread.h>
#include<mutex>
#include<condition_variable>
#include <list>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "client.h"
#include <cstring>

using namespace std;

#define MAX_CLIENTS 100
bool clientquitcmd = false;

void* worker_loop(void *arg)
{
	int *c_sfd = (int*)arg;
	int csfd = *c_sfd;
	char buff[4096];
	int n;
	fd_set readfd;
	fd_set treadfd;
	int max_fd;

	max_fd = csfd;
	FD_CLR(csfd, &readfd);
	FD_SET(csfd, &readfd);
	//fprintf(stderr, "the client socked fd isnide the worker_loop is %d\n", *c_sfd);
	while (1) {

		treadfd = readfd;
		if (select(max_fd + 1, &treadfd, NULL, NULL, NULL) < 0) {
			fprintf(stderr, "failed in select call");
		}

		if (FD_ISSET(csfd, &treadfd)) {

			memset(buff, 0, sizeof(buff));
			//read and write calls are atomic, the atomicity of these calls handled by the library implementaion.
			n = read(*c_sfd, buff, sizeof(buff));
			if(n == 0) {
				fprintf(stderr, "Looks the server terminated, so exiting\n");
				close(*c_sfd);
				*c_sfd = -1;
				break;
			} else if (n < 0) {
				fprintf(stderr, "Error in reading from the client socket, Ignoring\n");
				continue;
			}
			//fprintf(stderr, "response returned from the server is: %s\n", buff);
			fprintf(stderr, "%s\n", buff);
			if ( clientquitcmd ) {
				//if (strstr(buff, "QUIT") != NULL) {
				//found QUIT in return message from server
				close(*c_sfd);
				break;
				//}
			}
			fflush(stdout);
			fflush(stdout);
		}
	}

	exit(0);
}

void read_conf_file(char *file_name)
{
	FILE *fp;
	char buffer[MAXLINE];
	char *token;

	fp = fopen(file_name, "r");
	if(!fp) {
		cout<<"Error in opening input conf file: "<<file_name<<endl;
		goto out;
	}
	fscanf(fp, "%s", buffer);
	token = strtok(buffer, "=\n");
	if (strcmp(token, "SERVER_IP")) {
		cout<<"Invalid string in client.conf file, it should be SERVER_IP=<ip-adress>"<<endl;
		goto out;
	}
	while(token != NULL) {
		token = strtok(NULL, "=\n");
		if (token == NULL) break;
		memcpy(server_ip, token, strlen(token));
	}
	memset(buffer, 0, sizeof(buffer));
	fscanf(fp, "%s", buffer);
	token = strtok(buffer, "=\n");
	if (strcmp(token, "SERVER_PORT")) {
		cout<<"Invalid string in client.conf file, it should be SERVER_PORT=<server-port-number>"<<endl;
		goto out;
	}
	while(token != NULL) {
		token = strtok(NULL, "=\n");
		if (token == NULL) break;
		server_port = atoi(token);
	}

out:
	fclose(fp);

	return;
}

void start_client(void)
{
	int csfd;
	struct sockaddr_in serv_addr;
	struct sockaddr_in client_address;

	csfd = socket(AF_INET, SOCK_STREAM, 0);
	if (csfd < 0) {
		cout <<"Failed to create client socket fd"<<endl;
		exit(1);
	}

	//cout <<"the client socket fd: "<<csfd<<endl;

	//The below commented code is not mandatory, you can directly call connect without before calling bind for client
	//The OS by-default does all things, OS will choose a unique random port for your socket and bind the client socket to this port
	//you don't need to worry to call bind() before connect() in client side.
	//in some cases people does explicitly when they want to utilize more then 65K port, in that case we can ask OS to reuse the address
	//and so call exmplicitly bind.

	//Note: client and server communicates using the uique tuple:
	//  (Source IP, Source Port, Server IP, Server Port, Protocol)
	// this tuple needs to be unique i.e. any one of the value needs to be unique for each connection.
	// when we do a conect the OS uses (source IP, and unique source port for each client, server IP, server port, protocol)
	// Note, in this case for all the connections the uniqueness is achieved, because the OS binds the client socket at unique
	// port to make a unique tuple for each connection. Please not for each conenction the server IP and server port will be same
	// so the server IP and server port doesn'r provides the unique tuple, it is source port, when all the clients are running from the
	// same machine different terminals or source IP if all the clients are running from the different machines, provides the unique
	// tuple for each connection i.e. the client IP and/or client port number which helps to provides the unique tuple for each of the
	// connection establishde between client and the server machines.

	//We need to bind the client in local random unique port with the help of OS, so that
	//multiple client can be run from the same machine's multiple terminals
    	//fill the struct sockaddr_in to fill the struct used to bind this socket to a port
	//give sin_port = 0; it will ask OS to choose random unique port for the client.
        //client_address.sin_family = AF_INET;
        //client_address.sin_addr.s_addr = INADDR_ANY;
        //client_address.sin_port = htons(0);//let OS choose the random unique port for each client.
	//if (bind(csfd, (struct sockaddr *)&client_address, sizeof(client_address)) < 0){
		//cout<<"Client failed in its own binding"<<endl;
		//exit(1);
	//}

	//define csfd as blocking mode explicitly, so that read() call can block
	struct timeval read_timeout;
	read_timeout.tv_sec = 0;
	read_timeout.tv_usec = 0;
	if (setsockopt(csfd, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof(read_timeout)) < 0) {
		fprintf(stderr, "Failed to set read timeout on client socket fd\n");
		exit(0);
	}

	//fprintf(stderr, "The server ip is [%s] and server port is [%d]\n", server_ip, server_port);

	//now make a call to connect to the server port and IP address
	serv_addr.sin_family = AF_INET;         //ipv4
	//serv_addr.sin_addr.s_addr = inet_addr(server_ip);
	serv_addr.sin_port = htons(server_port);
	if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
		cout<<"Error in inet_pton"<<endl;
		exit(1);
	}
	if (connect(csfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		cout<<"error in connect"<<endl;
		exit(1);
	}

	string msg;
	char buff[4096];
	pthread_t tid;

	//create one client worker thread which will handle any read operation inside a separate thread
	//the reason we are keeping inside the separate thread because, in privmsg case the response may come
	//from server even when client doesn't sends the msg using the write() call, here getline() is also
	//blocking call to get the user input which need to handle inside main client thread, and all the response
	//from the client read() blocking call inside a separate client worker thrread.
	//basically, in client main thread we are blocking for user input in getline() blocking call and
	//in client worker thread we are blocking for server response in read() block call.

	pthread_create(&tid, NULL, worker_loop, (void*)&csfd);

	while (1) {
		
		cout <<"Enter the messages\n"<<endl;
		getline(cin, msg);
	
		//send a message to server
		memset(buff, 0, sizeof(buff));
		strcpy(buff, msg.c_str());
		if (strstr(buff, "QUIT") != NULL) {
			clientquitcmd = true;
		}
		//cout <<"The message string from the client is: "<<buff<<endl;

		//send query to server
		write(csfd, buff, strlen(buff));

	}

}


int main(int argc, char *argv[]) {

	if (argc != 2) {
		cout<<"Please provide client.conf as input of server program"<<endl;
		exit(1);
	}
	read_conf_file(argv[1]);

	start_client();

	return 0;
}
