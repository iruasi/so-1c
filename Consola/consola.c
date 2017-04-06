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

#define IP_KERNEL "127.0.0.1" // estos dos defines despues tienen que estar en un .config o algo
#define PUERTO_KERNEL "5010"

#define FALLO -21

void setupHints(struct addrinfo *hints, int address_family, int socket_type, int flags);

int main(void){

	char * msj = "Hola! Soy proceso Consola\n";
	int lenMsj = strlen(msj) + 1;
	int bytesSent;

	int stat;
	int sock_kern; // file descriptor para el socket del kernel
	struct addrinfo hints, *serverInfo;

	setupHints(&hints, AF_UNSPEC, SOCK_STREAM, 0);

	if ((stat = getaddrinfo(IP_KERNEL, PUERTO_KERNEL, &hints, &serverInfo)) != 0){
		fprintf("getaddrinfo: %s\n", gai_strerror(stat));
		return FALLO;
	}

	sock_kern = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol);
	freeaddrinfo(serverInfo);

// Podemos permitir este connect, no va a suceder nada: el puerto no lo esta atendiendo nadie...
	connect(sock_kern, servInfo->ai_addr, servInfo->ai_addrlen);

// Este send no produce efecto en nada porque no hubo conexion con un listener al sock_kern...
	bytesSent = send(sock_kern, msj, lenMsj, 0);

	close(sock_kern);
	return 0;
}


void setupHints(struct addrinfo *hint, int fam, int sockType, int flags){

	memset(hint, 0, sizeof *hint);
	hint->ai_family = fam;
	hint->ai_socktype = sockType;
	hint->ai_flags = flags;
}
