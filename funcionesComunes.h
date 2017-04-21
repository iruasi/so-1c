#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>

#define FALLO_GRAL -21
#define FALLO_CONEXION -24

#ifndef FUNCIONESCOMUNES_H_
#define FUNCIONESCOMUNES_H_

#endif /* FUNCIONESCOMUNES_H_ */

/* Recibe una estructura que almacena informacion del propio host;
 * La inicializa con valores utiles, pasados por parametro
 */
void setupHints(struct addrinfo *hints, int address_family, int socket_type, int flags){
    memset(hints, 0, sizeof *hints);
	hints->ai_family = address_family;
	hints->ai_socktype = socket_type;
	hints->ai_flags = flags;
}

/* Dados un ip y puerto de destino, se crea, conecta y retorna socket apto para comunicacion
 * La deberia utilizar unicamente Iniciar_Programa, por cada nuevo hilo para un script que se crea
 */
int establecerConexion(char *ip_dest, char *port_dest){

	int stat;
	int sock_dest; // file descriptor para el socket del destino a conectar
	struct addrinfo hints, *destInfo;

	setupHints(&hints, AF_UNSPEC, SOCK_STREAM, 0);

	if ((stat = getaddrinfo(ip_dest, port_dest, &hints, &destInfo)) != 0){
		fprintf("getaddrinfo: %s\n", gai_strerror(stat));
		return FALLO_GRAL;
	}

	if ((sock_dest = socket(destInfo->ai_family, destInfo->ai_socktype, destInfo->ai_protocol)) < 0)
		return FALLO_GRAL;

	connect(sock_dest, destInfo->ai_addr, destInfo->ai_addrlen);
	freeaddrinfo(destInfo);

	if (sock_dest < 0){
		printf("Error al tratar de conectar con Kernel!");
		return FALLO_CONEXION;
	}

	return sock_dest;
}

/* crea un socket y lo bindea() a un puerto particular,
 * luego retorna este socket, apto para listen()
 */
int makeListenSock(char *port_listen){

	int stat, sock_listen;

	struct addrinfo hints, *serverInfo;

	setupHints(&hints, AF_UNSPEC, SOCK_STREAM, AI_PASSIVE);

	if ((stat = getaddrinfo(NULL, port_listen, &hints, &serverInfo)) != 0){
		fprintf("getaddrinfo: %s\n", gai_strerror(stat));
		return FALLO_GRAL;
	}

	sock_listen = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol);
	bind(sock_listen, serverInfo->ai_addr, serverInfo->ai_addrlen);

	freeaddrinfo(serverInfo);
	return sock_listen;
}

/* acepta una conexion entrante, y crea un socket para comunicaciones regulares;
 */
int makeCommSock(int socket_in){

	struct sockaddr_in clientAddr;
	socklen_t clientSize = sizeof(clientAddr);

	int sock_comm = accept(socket_in, (struct sockaddr*) &clientAddr, &clientSize);

	return sock_comm;
}

/* Limpia un buffer usado por cada proceso para emitir varios mensajes
 */
void clearBuffer(char * buffer, int bufferLength){

	int i;
	for (i = 0; i < bufferLength; ++i)
		buffer[i] = '\0';

}
