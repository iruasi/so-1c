#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

#include <commons/config.h>
#include <commons/string.h>
#include <commons/log.h>
#include <commons/collections/list.h>

#include <funcionesCompartidas/funcionesCompartidas.h>
#include <tiposRecursos/tiposErrores.h>
#include <tiposRecursos/tiposPaquetes.h>

#include "fileSystemConfigurators.h"
#include "operacionesFS.h"

#ifndef BACKLOG
#define BACKLOG 20
#endif

int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	tPackHeader *head_tmp = malloc(sizeof *head_tmp);
	int stat;

	tFileSystem* fileSystem = getConfigFS(argv[1]);
	mostrarConfiguracion(fileSystem);


	int sock_lis_kern, sock_kern;
	if ((sock_lis_kern = makeListenSock(fileSystem->puerto_entrada)) < 0){
		printf("No se pudo crear socket listen en puerto: %s", fileSystem->puerto_entrada);
	}

	if((stat = listen(sock_lis_kern, BACKLOG)) == -1){
		perror("Fallo de listen sobre socket Kernel. error");
		return FALLO_GRAL;
	}

	if((sock_kern = makeCommSock(sock_lis_kern)) < 0){
		puts("No se pudo acceptar conexion entrante del Kernel");
		return FALLO_GRAL;
	}

	puts("Esperando handshake de Kernel...");
	while ((stat = recv(sock_kern, head_tmp, sizeof *head_tmp, 0)) > 0){

		printf("Se recibieron %d bytes\n", stat);
		printf("Emisor: %d\nTipo de mensaje: %d", head_tmp->tipo_de_mensaje, head_tmp->tipo_de_mensaje);
	}

	if (stat == -1){
		perror("Error en la realizacion de handshake con Kernel. error");
		return FALLO_RECV;
	}


	printf("Kernel termino la conexion\nLimpiando proceso...\n");
	close(sock_kern);
	liberarConfiguracionFileSystem(fileSystem);
	return EXIT_SUCCESS;
}







