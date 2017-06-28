#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>

#include <tiposRecursos/misc/pcb.h>
#include <tiposRecursos/tiposErrores.h>
#include <tiposRecursos/tiposPaquetes.h>
#include "funcionesCompartidas.h"

#define BACKLOG 20

bool assertEq(int expected, int actual, const char* errmsg){
	if (expected != actual){
		fprintf(stderr, "%s\n", errmsg);
		fprintf(stderr, "Error. Se esperaba %d, se obtuvo %d\n", expected, actual);
		return false;
	}
	return true;
}

void freeAndNULL(void **ptr){
	free(*ptr);
	*ptr = NULL;
}

unsigned long fsize(FILE* f){

    fseek(f, 0, SEEK_END);
    unsigned long len = (unsigned long) ftell(f);
    fseek(f, 0, SEEK_SET);
    return len;

}

void setupHints(struct addrinfo *hints, int address_family, int socket_type, int flags){
    memset(hints, 0, sizeof *hints);
	hints->ai_family = address_family;
	hints->ai_socktype = socket_type;
	hints->ai_flags = flags;
}

int handshakeCon(int sock_dest, int id_sender){

	int stat;
	char *package;
	tPackHeader head;
	head.tipo_de_proceso = id_sender;
	head.tipo_de_mensaje = HSHAKE;

	if ((package = malloc(HEAD_SIZE)) == NULL){
		fprintf(stderr, "No se pudo hacer malloc\n");
		return FALLO_GRAL;
	}
	memcpy(package, &head, HEAD_SIZE);

	if ((stat = send(sock_dest, package, HEAD_SIZE, 0)) == -1){
		perror("Fallo send de handshake. error");
		printf("Fallo send() al socket: %d\n", sock_dest);
		return FALLO_SEND;
	}

	free(package);
	return stat;
}

int establecerConexion(char *ip_dest, char *port_dest){

	int stat;
	int sock_dest; // file descriptor para el socket del destino a conectar
	struct addrinfo hints, *destInfo;

	setupHints(&hints, AF_INET, SOCK_STREAM, 0);

	if ((stat = getaddrinfo(ip_dest, port_dest, &hints, &destInfo)) != 0){
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(stat));
		return FALLO_GRAL;
	}

	if ((sock_dest = socket(destInfo->ai_family, destInfo->ai_socktype, destInfo->ai_protocol)) == -1){
		perror("No se pudo crear socket. error.");
		return FALLO_GRAL;
	}

	if ((stat = connect(sock_dest, destInfo->ai_addr, destInfo->ai_addrlen)) == -1){
		perror("No se pudo establecer conexion, fallo connect(). error");
		printf("Fallo conexion con destino IP: %s PORT: %s\n", ip_dest, port_dest);
		return FALLO_CONEXION;
	}

	freeaddrinfo(destInfo);

	if (sock_dest < 0){
		printf("Error al tratar de conectar con Kernel!\n");
		return FALLO_CONEXION;
	}

	return sock_dest;
}

int makeListenSock(char *port_listen){

	int stat, sock_listen;
	struct addrinfo hints, *serverInfo;

	setupHints(&hints, AF_INET, SOCK_STREAM, AI_PASSIVE);

	if ((stat = getaddrinfo(NULL, port_listen, &hints, &serverInfo)) != 0){
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(stat));
		return FALLO_GRAL;
	}

	if ((sock_listen = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol)) == -1){
		perror("No se pudo crear socket. error.");
		return FALLO_GRAL;
	}
	if ((bind(sock_listen, serverInfo->ai_addr, serverInfo->ai_addrlen)) == -1){
		perror("Fallo binding con socket. error");
		printf("Fallo bind() en PORT: %s\n", port_listen);
		return FALLO_CONEXION;
	}

	freeaddrinfo(serverInfo);
	return sock_listen;
}

int makeCommSock(int socket_in){

	int sock_comm;
	struct sockaddr_in clientAddr;
	socklen_t clientSize = sizeof(clientAddr);

	if ((sock_comm = accept(socket_in, (struct sockaddr*) &clientAddr, &clientSize)) == -1){
		perror("Fallo accept del socket entrada. error");
		printf("Fallo accept() en socket_in: %d\n", socket_in);
		return FALLO_CONEXION;
	}

	return sock_comm;
}

int handleNewListened(int sock_listen, fd_set *setFD){

	int stat;
	int new_fd = makeCommSock(sock_listen);
	FD_SET(new_fd, setFD);

	if ((stat = listen(sock_listen, BACKLOG)) == -1){
		perror("Fallo listen al socket. error");
		printf("Fallo listen() en el sock_listen: %d", sock_listen);
		return FALLO_GRAL;
	}

	return new_fd;
}

void clearBuffer(char * buffer, int bufferLength){

	int i;
	for (i = 0; i < bufferLength; ++i)
		buffer[i] = '\0';

}

void clearAndClose(int *fd, fd_set *setFD){
	FD_CLR(*fd, setFD);
	close(*fd);
}


int cantidadTotalDeBytesRecibidos(int fdServidor, char *buffer, int tamanioBytes) { //Esta función va en funcionesCompartidas
	int total = 0;
	int bytes_recibidos;

	while (total < tamanioBytes){

	bytes_recibidos = recv(fdServidor, buffer+total, tamanioBytes, MSG_WAITALL);
	// MSG_WAITALL: el recv queda completamente bloqueado hasta que el paquete sea recibido completamente

	if (bytes_recibidos == -1) { // Error al recibir mensaje
		perror("[SOCKETS] No se pudo recibir correctamente los datos.\n");
		break;
			}

	if (bytes_recibidos == 0) { // Conexión cerrada
		printf("[SOCKETS] La conexión fd #%d se ha cerrado.\n", fdServidor);
		break;
	}
	total += bytes_recibidos;
	tamanioBytes -= bytes_recibidos;
		}
	return bytes_recibidos; // En caso de éxito, se retorna la cantidad de bytes realmente recibida
}


indiceStack *crearStackVacio(void){
	indiceStack *stack = malloc(sizeof *stack);

	stack->args = list_create();
	stack->vars = list_create();
	stack->retPos = -1;
	stack->retVar.pag    = -1;
	stack->retVar.offset = -1;
	stack->retVar.size   = -1;

	return stack;
}
