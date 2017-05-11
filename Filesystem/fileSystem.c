#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <commons/config.h>
#include <commons/string.h>
#include <commons/log.h>
#include <commons/collections/list.h>

#include "../Compartidas/funcionesCompartidas.c"
#include "../Compartidas/tiposErrores.h"
#include "fileSystemConfigurators.h"

#ifndef BACKLOG
#define BACKLOG 20
#endif

int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	char buf[MAXMSJ];
	int stat;

	tFileSystem* fileSystem = getConfigFS(argv[1]);
	mostrarConfiguracion(fileSystem);

	//SV SIMPLE PARA QUE SE LE CONECTE KERNEL

	// Creamos sockets listos para send/recv para cada proceso
	int sock_lis_kern = makeListenSock(fileSystem->puerto_entrada);
	listen(sock_lis_kern, BACKLOG);
	int sock_kern = makeCommSock(sock_lis_kern);


	while ((stat = recv(sock_kern, buf, MAXMSJ, 0)) > 0){

		printf("%s\n", buf);
		clearBuffer(buf, MAXMSJ);
	}


	if (stat < 0){
		printf("Error en la recepcion de datos!\n valor retorno recv: %d \n", stat);
		return FALLO_RECV;
	}



	//FIN SV SIMPLE

	printf("Kernel termino la conexion\nLimpiando proceso...\n");
	close(sock_kern);
	liberarConfiguracionFileSystem(fileSystem);
	return EXIT_SUCCESS;
}







