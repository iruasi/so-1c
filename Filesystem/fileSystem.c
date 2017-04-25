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
#include "fileSystem.h"

#define BACKLOG 10


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



tFileSystem* getConfigFS(char* ruta){
	printf("Ruta del archivo de configuracion: %s\n", ruta);
	tFileSystem *fileSystem= malloc(sizeof(tFileSystem));

	fileSystem->puerto_entrada = malloc(MAX_PORT_LEN);
	fileSystem->punto_montaje  = malloc(MAX_IP_LEN);
	fileSystem->ip_kernel      = malloc(MAX_IP_LEN);

	t_config *fileSystemConfig = config_create(ruta);

	strcpy(fileSystem->puerto_entrada, config_get_string_value(fileSystemConfig, "PUERTO_ENTRADA"));
	strcpy(fileSystem->punto_montaje,  config_get_string_value(fileSystemConfig, "PUNTO_MONTAJE"));
	strcpy(fileSystem->ip_kernel,      config_get_string_value(fileSystemConfig, "IP_KERNEL"));

	config_destroy(fileSystemConfig);
	return fileSystem;
}

void mostrarConfiguracion(tFileSystem *fileSystem){

	printf("Puerto: %s\n",           fileSystem->puerto_entrada);
	printf("Punto de montaje: %s\n", fileSystem->punto_montaje);
	printf("IP del kernel: %s\n",    fileSystem->ip_kernel);
}

void liberarConfiguracionFileSystem(tFileSystem *fileSystem){

	free(fileSystem->punto_montaje);
	free(fileSystem->ip_kernel);
	free(fileSystem->puerto_entrada);
	free(fileSystem);
}
