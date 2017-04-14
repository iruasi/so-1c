#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>

#include <commons/config.h>
#include <commons/string.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include "kernel.h"


#define BACKLOG 20

#define MAX_IP_LEN 16   // aaa.bbb.ccc.ddd -> son 15 caracteres, 16 contando un '\0'
#define MAX_PORT_LEN 6  // 65535 -> 5 digitos, 6 contando un '\0'
#define MAXMSJ 100

#define FALLO_GRAL -21
#define FALLO_CONFIGURACION -22
#define FALLO_RECV -23

void setupHints(struct addrinfo *hint, int address_family, int socket_type, int flags);
int setSocketComm(char *puerto_de_comunicacion);

void mostrarConfiguracion(tKernel *datos_kernel);
void liberarConfiguracionKernel(tKernel *datos_kernel);

int main(int argc, char* argv[]){
	if(argc!=2){
			printf("Error en la cantidad de parametros\n");
			return EXIT_FAILURE;
	}

	char buf[MAXMSJ];
	int stat, bytes_sent;
	int i;

	struct sockaddr_in clientInfo;
	socklen_t clientSize = sizeof(clientInfo);

	tKernel *kernel = getConfigKernel(argv[1]);
	mostrarConfiguracion(kernel);

	// Creamos sockets listos para send/recv para cada proceso
	int fd_cons = setSocketComm(kernel->puerto_prog);
	int fd_cpu  = setSocketComm(kernel->puerto_cpu);
	int fd_mem  = setSocketComm(kernel->puerto_memoria);
	int fd_fs   = setSocketComm(kernel->puerto_fs);

	while ((stat = recv(fd_cons, buf, MAXMSJ, 0)) > 0){
		stat = send(fd_cpu, buf, MAXMSJ, 0);

		stat = send(fd_mem, buf, MAXMSJ, 0);

		stat = send(fd_fs, buf, MAXMSJ, 0);

		for (i = 0; i < MAXMSJ; ++i)
			buf[i] = '\0';
	}

	if (bytes_sent == -1){
		printf("Error en la recepcion de datos!\n valor retorno recv: %d \n", bytes_sent);
		return FALLO_RECV;
	}

	printf("Consola termino la conexion\nNo se esperan mas mensajes, cerrando todo...\n");

	close(fd_cons);
	close(fd_cpu);
	close(fd_mem);
	close(fd_fs);

	liberarConfiguracionKernel(kernel);
	return stat;
}

void setupHints(struct addrinfo *hint, int fam, int sockType, int flags){

	memset(hint, 0, sizeof *hint);
	hint->ai_family = fam;
	hint->ai_socktype = sockType;
	hint->ai_flags = flags;
}

/* Crea un socket para establecer comunicacion, a partir de un puerto dado
 */
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
	sock_comm = accept(sock_listen, (struct sockaddr *) &clientInfo, &clientSize);
	close(sock_listen);

	return sock_comm;
}


tKernel *getConfigKernel(char* ruta){

	printf("Ruta del archivo de configuracion: %s\n", ruta);
	tKernel *kernel = malloc(sizeof(tKernel));

	kernel->algoritmo   = malloc(2 * sizeof kernel->algoritmo);
	kernel->ip_fs       = malloc(MAX_IP_LEN * sizeof kernel->ip_fs);
	kernel->ip_memoria  = malloc(MAX_IP_LEN * sizeof kernel->ip_memoria);
	kernel->puerto_cpu  = malloc(MAX_PORT_LEN * sizeof kernel->puerto_cpu);
	kernel->puerto_prog = malloc(MAX_PORT_LEN * sizeof kernel->puerto_prog);
	kernel->puerto_memoria = malloc(MAX_PORT_LEN * sizeof kernel->puerto_memoria);
	kernel->puerto_fs   = malloc(MAX_PORT_LEN * sizeof kernel->puerto_fs);

	t_config *kernelConfig = config_create(ruta);

	strcpy(kernel->algoritmo,  config_get_string_value(kernelConfig, "ALGORITMO"));
	strcpy(kernel->ip_fs,      config_get_string_value(kernelConfig, "IP_FS"));
	strcpy(kernel->ip_memoria, config_get_string_value(kernelConfig, "IP_MEMORIA"));
	strcpy(kernel->puerto_prog,    config_get_string_value(kernelConfig, "PUERTO_PROG"));
	strcpy(kernel->puerto_cpu, config_get_string_value(kernelConfig, "PUERTO_CPU"));
	strcpy(kernel->puerto_memoria, config_get_string_value(kernelConfig, "PUERTO_MEMORIA"));
	strcpy(kernel->puerto_fs,  config_get_string_value(kernelConfig, "PUERTO_FS"));

	kernel->grado_multiprog = config_get_int_value(kernelConfig, "GRADO_MULTIPROG");
	kernel->quantum         = config_get_int_value(kernelConfig, "QUANTUM");
	kernel->quantum_sleep   = config_get_int_value(kernelConfig, "QUANTUM_SLEEP");
	kernel->stack_size      = config_get_int_value(kernelConfig, "STACK_SIZE");
	kernel->sem_ids         = config_get_array_value(kernelConfig, "SEM_IDS");
	kernel->sem_init        = config_get_array_value(kernelConfig, "SEM_INIT");
	kernel->shared_vars     = config_get_array_value(kernelConfig, "SHARED_VARS");

	config_destroy(kernelConfig);
	return kernel;

}


// todo: hacer bien el recorrido de vectores
void mostrarConfiguracion(tKernel *kernel){

	printf("Algoritmo: %s\n", kernel->algoritmo);
	printf("Grado de multiprogramacion: %d\n", kernel->grado_multiprog);
	printf("IP para el File System: %s\n", kernel->ip_fs);
	printf("IP para la memoria: %s\n", kernel->ip_memoria);
	printf("Puerto CPU: %s\n", kernel->puerto_cpu);
	printf("Puerto para la memoria: %s\n", kernel->puerto_memoria);
	printf("Puerto para los programas: %s\n", kernel->puerto_prog);
	printf("Puerto para el filesystem: %s\n", kernel->puerto_fs);
	printf("Quantum: %d\n", kernel->quantum);
	printf("Quantum Sleep: %d\n", kernel->quantum_sleep);
	printf("Stack size: %d\n", kernel->stack_size);
	printf("Identificadores de semaforos: %s, %s, %s\n", kernel->sem_ids[0], kernel->sem_ids[1], kernel->sem_ids[2]);
	printf("Valores de semaforos: %s, %s, %s\n", kernel->sem_init[0], kernel->sem_init[1], kernel->sem_init[2]);
	printf("Variables globales: %s, %s, %s\n", kernel->shared_vars[0], kernel->shared_vars[1], kernel->shared_vars[2]);

}

void liberarConfiguracionKernel(tKernel *kernel){

	free(kernel->algoritmo);
	free(kernel->ip_fs);
	free(kernel->ip_memoria);
	free(kernel->puerto_cpu);
	free(kernel->puerto_memoria);
	free(kernel->puerto_prog);
	free(kernel->puerto_fs);
	free(kernel);
}
