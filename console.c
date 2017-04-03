/*
 Estos includes los saque de la guia Beej... puede que ni siquiera los precisemos,
 pero los dejo aca para futuro, si algo mas complejo no anda tal vez sirvan...

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
*/

#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include </home/utnso/workspace/consola/errorCodes.h>

#define IP_KERNEL "127.0.0.1" // estos dos defines despues tienen que estar en un .config o algo
#define PUERTO_KERNEL 5010

void setupHints(struct addrinfo *h, int fam, int sock, int flag);
int socketYConnect(struct addrinfo *h, struct addrinfo *r);

int main(void){

	char * msj = "Hola! Soy proceso Consola\n";
	int lenMsj = strlen(msj);
	int bytes_sent;

	int status;
	int sockfd; // file descriptor para el socket
	struct addrinfo hints, *res;

	// Prepara en hints una estructura con ipv4 para TCP
	setupHints(&hints, AF_INET, SOCK_STREAM, AI_PASSIVE);

	sockfd = socketYConnect(&hints, res);

// Por ahora no utilizamos send() porque precisamos el kernel funcione
//	bytes_sent = send(sockfd, msj, lenMsj, 0);

	printf("el file descriptor del socket es %d\n", sockfd);

	return 0;
}

int socketYConnect(struct addrinfo *hints, struct addrinfo *res){

	int stat, socketfd;

	if ((stat = getaddrinfo("127.0.0.1", NULL, hints, &res)) != 0){
		fprintf("getaddrinfo: %s\n", gai_strerror(stat));
		return ERR_FUNC_NATIVA;
	}

	socketfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

// Por ahora no utilizamos connect() porque precisamos que el kernel funcione
//	connect(socketfd, res->ai_addr, res->ai_addrlen);

	return socketfd;
}


void setupHints(struct addrinfo *hint, int fam, int socketType, int flags){

	memset(hint, 0, sizeof *hint);
	hint->ai_family = fam;
	hint->ai_socktype = socketType;
	hint->ai_flags = flags;
}

