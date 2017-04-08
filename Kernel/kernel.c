#include <stdio.h>
#include<stdlib.h>
#include <string.h>
#include <netdb.h>
#include <commons/config.h>
#include <commons/string.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include "kernel.h"

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

int main(int argc, char* argv[]){
	if(argc!=2){
			printf("Error en la cantidad de parametros\n");
			return EXIT_FAILURE;
	}
	tKernel *kernel = getConfigKernel(argv[1]);
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

tKernel *getConfigKernel(char* ruta){
	printf("Ruta del archivo de configuracion: %s\n", ruta);
	tKernel *kernel = malloc(sizeof(tKernel));
	t_config *kernelConfig = config_create(ruta);
	kernel->algoritmo = config_get_string_value(kernelConfig, "ALGORITMO");
	kernel->grado_multiprog = config_get_int_value(kernelConfig, "GRADO_MULTIPROG");
	kernel->ip_fs = config_get_string_value(kernelConfig, "IP_FS");
	kernel->ip_memoria = config_get_string_value(kernelConfig, "IP_MEMORIA");
	kernel->puerto_cpu = config_get_int_value(kernelConfig, "PUERTO_CPU");
	kernel->puerto_memoria = config_get_int_value(kernelConfig, "PUERTO_MEMORIA");
	kernel->puerto_prog = config_get_int_value(kernelConfig, "PUERTO_PROG");
	kernel->quantum = config_get_int_value(kernelConfig, "QUANTUM");
	kernel->quantum_sleep = config_get_int_value(kernelConfig, "QUANTUM_SLEEP");
	kernel->stack_size = config_get_int_value(kernelConfig, "STACK_SIZE");
	kernel->sem_ids = config_get_array_value(kernelConfig, "SEM_IDS");
	kernel->sem_init = config_get_array_value(kernelConfig, "SEM_INIT");
	kernel->shared_vars = config_get_array_value(kernelConfig, "SHARED_VARS");

	printf("Algoritmo: %s\n", kernel->algoritmo);
	printf("Grado de multiprogramacion: %d\n", kernel->grado_multiprog);
	printf("IP para el File System: %s\n", kernel->ip_fs);
	printf("IP para la memoria: %s\n", kernel->ip_memoria);
	printf("Puerto CPU: %d\n", kernel->puerto_cpu);
	printf("Puerto para la memoria: %d\n", kernel->puerto_memoria);
	printf("Puerto para los programas: %d\n", kernel->puerto_prog);
	printf("Quantum: %d\n", kernel->quantum);
	printf("Quantum Sleep: %d\n", kernel->quantum_sleep);
	printf("Stack size: %d\n", kernel->stack_size);
	printf("Identificadores de semaforos: ");
	//HABRIA QUE HACER UNA FUNCION DESDE ACA
	int x=0;
	while(kernel->sem_ids[x]){
		if (kernel->sem_ids[x+1] == NULL){
				kernel->sem_ids[x][strlen(kernel->sem_ids[x])-1]='\0';
			}
		printf("%s ", kernel->sem_ids[x]);
		x++;
	}
	//HASTA ACA
	x=0;
	printf("\nValores iniciales de cada semaforo:");
	while(kernel->sem_init[x]){
		if (kernel->sem_init[x+1] == NULL){
						kernel->sem_init[x][strlen(kernel->sem_init[x])-1]='\0';
		}
		printf("%s ", kernel->sem_init[x]);
		x++;
	}
	printf("\nVariables compartidas: ");
	x=0;
	while(kernel->shared_vars[x]){
		if (kernel->shared_vars[x+1] == NULL){
				kernel->shared_vars[x][strlen(kernel->shared_vars[x])-1]='\0';
			}
		printf("%s ", kernel->shared_vars[x]);
		x++;
	}

	config_destroy(kernelConfig);
	return kernel;

}
