#include <stdio.h>
#include <stdlib.h>
#include "fileSystem.h"
#include <commons/config.h>
#include <commons/string.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include <string.h>
#include <netdb.h>

#define BACKLOG 10

int establecerConexion(char *ip_destino, char *puerto_destino);
int setSocketComm(char *puerto_de_comunicacion);


int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	char buf[MAXMSJ];
	int i;
	int stat;
	int sock_kern;
	int bytes_sent;

	tFileSystem* fileSystem = getConfigFS(argv[1]);
	mostrarConfiguracion(fileSystem);

	//SV SIMPLE PARA QUE SE LE CONECTE KERNEL

	// Creamos sockets listos para send/recv para cada proceso
	int fd_kernel = setSocketComm(fileSystem->puerto);


	while ((stat = recv(fd_kernel, buf, MAXMSJ, 0)) > 0){

		printf("%s\n", buf);

		for (i = 0; i < MAXMSJ; ++i)
			buf[i] = '\0';
		}

		if (bytes_sent == -1){
			printf("Error en la recepcion de datos!\n valor retorno recv: %d \n", bytes_sent);
			return FALLO_RECV;
		}






	//FIN SV SIMPLE









	/*printf("Conectando con kernel...\n");
	sock_kern = establecerConexion(fileSystem->ip_kernel, fileSystem->puerto);
	if (sock_kern < 0)
		return sock_kern;

	while((stat = recv(sock_kern, buf, MAXMSJ, 0)) > 0){
		printf("%s\n", buf);

		for(i = 0; i < MAXMSJ; ++i)
			buf[i] = '\0';
	}

	if (stat == -1){
		printf("Error en la conexion con Kernel! status: %d \n", stat);
		return -1;
	}

	printf("Kernel termino la conexion\nLimpiando proceso...\n");

	close(sock_kern);*/
	liberarConfiguracionFileSystem(fileSystem);
	return EXIT_SUCCESS;
}

void setupHints(struct addrinfo *hint, int fam, int sockType, int flags){

	memset(hint, 0, sizeof *hint);
	hint->ai_family = fam;
	hint->ai_socktype = sockType;
	hint->ai_flags = flags;
}


/* Dados un ip y puerto de destino, se crea, conecta y retorna socket apto para comunicacion
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

tFileSystem* getConfigFS(char* ruta){
	printf("Ruta del archivo de configuracion: %s\n", ruta);
	tFileSystem *fileSystem= malloc(sizeof(tFileSystem));

	fileSystem->puerto        = malloc(MAX_PORT_LEN * sizeof(fileSystem->puerto));
	fileSystem->punto_montaje = malloc(MAX_IP_LEN * sizeof(fileSystem->punto_montaje));
	fileSystem->ip_kernel     = malloc(MAX_IP_LEN * sizeof(fileSystem->ip_kernel));
	//fileSystem->puerto_kernel = malloc(MAX_PORT_LEN * sizeof(fileSystem->puerto_kernel));

	t_config *fileSystemConfig = config_create(ruta);

	strcpy(fileSystem->puerto, config_get_string_value(fileSystemConfig, "PUERTO"));
	//strcpy(fileSystem->puerto_kernel, config_get_string_value(fileSystemConfig, "PUERTO_KERNEL"));
	strcpy(fileSystem->punto_montaje, config_get_string_value(fileSystemConfig, "PUNTO_MONTAJE"));
	strcpy(fileSystem->ip_kernel, config_get_string_value(fileSystemConfig, "IP_KERNEL"));

	config_destroy(fileSystemConfig);
	return fileSystem;
}

void mostrarConfiguracion(tFileSystem *fileSystem){

	printf("Puerto: %s\n", fileSystem->puerto);
	printf("Punto de montaje: %s\n", fileSystem->punto_montaje);
	printf("IP del kernel: %s\n", fileSystem->ip_kernel);
	//printf("Puerto del kernel: %s\n", fileSystem->puerto_kernel);

}

void liberarConfiguracionFileSystem(tFileSystem *fileSystem){

	free(fileSystem->punto_montaje);
	free(fileSystem->ip_kernel);
	free(fileSystem->puerto);
	free(fileSystem);
}

int setSocketComm(char *port_comm){
        int stat;
        int sock_listen;
        struct addrinfo hints, *serverInfo;

        int sock_comm;
        struct sockaddr_in clientInfo;
        socklen_t clientSize = sizeof(clientInfo);

        setupHints(&hints, AF_UNSPEC, SOCK_STREAM, AI_PASSIVE);

        if ((stat = getaddrinfo(NULL, port_comm, &hints, &serverInfo)) != 0){
                fprintf("getaddrinfo: %s\n", gai_strerror(stat));
                return FALLO_GRAL;
        }

        sock_listen = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol);

        bind(sock_listen, serverInfo->ai_addr, serverInfo->ai_addrlen);
        freeaddrinfo(serverInfo);

        listen(sock_listen, BACKLOG);
        printf("Estoy escuchando...\n");
        sock_comm = accept(sock_listen, (struct sockaddr *) &clientInfo, &clientSize);
        printf("Devuelvo un sock_comm: %d, para el del puerto %s\n", sock_comm, port_comm);

        close(sock_listen);

        return sock_comm;
}
