#include <stdio.h>
#include <string.h>
#include <netdb.h>

#define PUERTO_INCOME "5010"
#define MAXCLIENTS 5

#define MAXMSJ 100

void setupHints(struct addrinfo *hint, int address_family, int socket_type, int flags);
int recibirConexion();

int recibirConexion(){

	char buf[MAXMSJ];

	int stat;
	int sock_listen;
	struct addrinfo hints, *serverInfo;

	int sock_cli;
	struct sockaddr_in clientInfo;
	socklen_t clientSize = sizeof(clientInfo);

	setupHints(&hints, AF_UNSPEC, SOCK_STREAM, AI_PASSIVE);

	if ((stat = getaddrinfo(NULL, PUERTO_INCOME, &hints, &serverInfo)) != 0){
		fprintf("getaddrinfo: %s\n", gai_strerror(stat));
		return -21;
	}

	sock_listen = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol);

	bind(sock_listen, serverInfo->ai_addr, serverInfo->ai_addrlen);
	freeaddrinfo(serverInfo);

	listen(sock_listen, MAXCLIENTS); // syscall bloqueante
	sock_cli = accept(sock_listen, (struct sockaddr *) &clientInfo, &clientSize);

	recv(sock_cli, buf, MAXMSJ, 0);
	printf ("%s\n", buf);

	return 0;
}

int main(void){

	int stat;

	stat = recibirConexion();

	return stat;
}

void setupHints(struct addrinfo *hint, int fam, int sockType, int flags){

	memset(hint, 0, sizeof *hint);
	hint->ai_family = fam;
	hint->ai_socktype = sockType;
	hint->ai_flags = flags;
}
