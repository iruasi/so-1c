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

char *eliminarWhitespace(char *string){

	int i;
	char *var = NULL;
	int var_len = strlen(string);

	for (i = 0; i < var_len; ++i){
		if (CHAR_WHITESPACE(string[i])){
			var = malloc(i + 1);
			memcpy(var, string, i);
			var[i] = '\0';
			return var;
		}
	}

	var = malloc(var_len + 1);
	memcpy(var, string, var_len);
	var[var_len] = '\0';
	return var;
}


bool assertEq(int expected, int actual, const char* errmsg){
	if (expected != actual){
		fprintf(stderr, "%s\n", errmsg);
		fprintf(stderr, "Error. Se esperaba %d, se obtuvo %d\n", expected, actual);
		return false;
	}
	return true;
}

int validarRespuesta(int sock, tPackHeader h_esp, tPackHeader *h_obt){

	if ((recv(sock, h_obt, HEAD_SIZE, 0)) == -1){
		perror("Fallo recv de Header. error");
		return FALLO_RECV;
	}

	if (h_esp.tipo_de_proceso != h_obt->tipo_de_proceso){
		printf("Fallo de comunicacion. Se espera un mensaje de %d, se recibio de %d\n",
				h_esp.tipo_de_proceso, h_obt->tipo_de_proceso);
		return FALLO_GRAL;
	}

	if (h_esp.tipo_de_mensaje != h_obt->tipo_de_mensaje){
		printf("Fallo ejecucion de funcion con valor %d\n", h_obt->tipo_de_mensaje);
		return FALLO_GRAL;
	}

	return 0;
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

void liberarPCB(tPCB *pcb){

	free(pcb->indiceDeCodigo);
	if (pcb->indiceDeEtiquetas) // if hay etiquetas de algun tipo
		free(pcb->indiceDeEtiquetas);
	liberarStack(pcb->indiceDeStack);
	freeAndNULL((void **) &pcb);
}

void liberarStack(t_list *stack_ind){

	int i, j;
	indiceStack *stack;

	for (i = 0; i < list_size(stack_ind); ++i){
		stack = list_remove(stack_ind, i);

		for (j = 0; j < list_size(stack->args); ++j)
			list_remove(stack->args, j);
		list_destroy(stack->args);

		for (j = 0; j < list_size(stack->vars); ++j)
			list_remove(stack->vars, j);
		list_destroy(stack->vars);
	}
	list_destroy(stack_ind);
}


int sendall(int sock, char *buff, int *len){

	int total = 0;
	int left  = *len;
	int stat;

	while (total < *len){
		if ((stat = send(sock, buff, left, 0)) == -1){
			perror("No se pudo sendall'ear el paquete. error");
			break;
		}
		total += stat;
		left  -= stat;
	}

	*len = total;
	return (stat == -1)? FALLO_SEND : 0;
}
