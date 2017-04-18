#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>

#include <commons/config.h>
#include <commons/string.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include "kernel.h"

#define BACKLOG 10
#define BACKLOG_CPU  10 // cantidad de procesos CPU que pueden enfilar en un solo listen
#define BACKLOG_CONS 10 // cantidad de procesos Consola que pueden enfilar en un solo listen

#define MAX_IP_LEN 16   // aaa.bbb.ccc.ddd -> son 15 caracteres, 16 contando un '\0'
#define MAX_PORT_LEN 6  // 65535 -> 5 digitos, 6 contando un '\0'
#define MAXMSJ 100
#define MAXALGO 4 		// cantidad maxima de carecteres para kernel->algoritmo (RR o FIFO)

#define FALLO_GRAL -21
#define FALLO_CONFIGURACION -22
#define FALLO_RECV -23
#define FALLO_CONEXION -24

/* MAX(A, B) es un macro que devuelve el mayor entre dos argumentos,
 * lo usamos para actualizar el maximo socket existente, a medida que se crean otros nuevos
 */
#define MAX(A, B) ((A) > (B) ? (A) : (B))

void setupHints(struct addrinfo *hint, int address_family, int socket_type, int flags);
int establecerConexion(char *ip_destino, char *port_destino);
int makeListenSock(char *port_listen);
int makeCommSock(int socket_listen);

void mostrarConfiguracion(tKernel *datos_kernel);
void liberarConfiguracionKernel(tKernel *datos_kernel);


int main(int argc, char* argv[]){
	if(argc!=2){
			printf("Error en la cantidad de parametros\n");
			return EXIT_FAILURE;
	}

	char buf[MAXMSJ];
	int stat, bytes_sent, ready_fds;
	int i, fd, new_fd;
	int sock_fs, sock_mem;
	int sock_lis_cpu, sock_lis_con;
	int fd_max = -1;

	// Creamos e inicializamos los conjuntos que retendran sockets para el select()
	fd_set read_fd, master_fd;
	FD_ZERO(&read_fd);
	FD_ZERO(&master_fd);

	struct sockaddr_in clientAddr;
	socklen_t clientSize = sizeof(clientAddr);

	tKernel *kernel = getConfigKernel(argv[1]);
	mostrarConfiguracion(kernel);


/* 		----DE MOMENTO NO EXISTE SERVIDOR EN MEMORIA, ASI QUE TODAS LAS COSAS RELACIONADAS A MEMORIA SE VERAN COMENTADAS----
	// Se trata de conectar con Memoria
	if ((sock_mem = establecerConexion(kernel->ip_memoria, kernel->puerto_memoria)) < 0){
		printf("Error fatal! No se pudo conectar con la Memoria! sock_mem: %d\n", sock_mem);
		return FALLO_CONEXION;
	}

	fd_max = MAX(sock_mem, fd_max);
*/

	// Se trata de conectar con Filesystem
	if ((sock_fs  = establecerConexion(kernel->ip_fs, kernel->puerto_fs)) < 0){
		printf("Error fatal! No se pudo conectar con el Filesystem! sock_fs: %d\n", sock_fs);
		return FALLO_CONEXION;
	}
	char * msjAFilesystem = "Hola soy Kernel";
	bytes_sent = send(sock_fs, msjAFilesystem, strlen(msjAFilesystem), 0);
	printf("Se enviaron: %d bytes\n a FiLESYSTEM", bytes_sent);


	fd_max = MAX(sock_fs, fd_max);

	// Creamos sockets para hacer listen() de CPUs
	if ((sock_lis_cpu = makeListenSock(kernel->puerto_cpu)) < 0){
		printf("No se pudo crear socket para escuchar! sock_lis_cpu: %d\n", sock_lis_cpu);
		return FALLO_CONEXION;
	}

	fd_max = MAX(sock_lis_cpu, fd_max);

	// Creamos sockets para hacer listen() de Consolas
	if ((sock_lis_con = makeListenSock(kernel->puerto_prog)) < 0){
		printf("No se pudo crear socket para escuchar! sock_lis_con: %d\n", sock_lis_con);
		return FALLO_CONEXION;
	}

	fd_max = MAX(sock_lis_con, fd_max);

	// fd_max ahora va a tener el valor mas alto de los sockets hasta ahora hechos
	// la implementacion sigue siendo ineficiente.. a futuro se va a hacer algo mas power!

	// Se agregan memoria, fs, listen_cpu, listen_consola y stdin al set master
	//FD_SET(sock_mem, &master_fd);
	FD_SET(sock_fs, &master_fd);
	FD_SET(sock_lis_cpu, &master_fd);
	FD_SET(sock_lis_con, &master_fd);
	FD_SET(0, &master_fd);


	listen(sock_lis_cpu, BACKLOG_CPU);
	listen(sock_lis_con, BACKLOG_CONS);

	// Para el select() vamos a estar pasando NULL por timeout, recomendacion de la catedra
	while (1){
		read_fd = master_fd;

		ready_fds = select(fd_max + 1, &read_fd, NULL, NULL, NULL);
		if (ready_fds == -1)
			return -26; // despues vemos de hacerlo lindo...

		//printf("lele");
		for (fd = 0; fd < fd_max; ++fd){

			if (FD_ISSET(fd, &read_fd)){
				printf("Aca hay uno! el fd es: %d\n", fd);

				// Para luego distinguir un recv() entre diferentes procesos, debemos definir un t_Package
				// este t_Package tendria un identificador, y un mensaje. Ambos de un size maximo predeterminado.
				if (fd == sock_lis_cpu){
					printf("es de cpu!\n");
					new_fd = makeCommSock(sock_lis_cpu);
					fd_max = MAX(fd_max, new_fd);

					FD_SET(new_fd, &master_fd);
					listen(sock_lis_cpu, BACKLOG_CPU); // Volvemos a ponernoas para escuchar otro CPU

				} else if (fd == sock_lis_con){
					printf("es de consola!\n");
					new_fd = makeCommSock(sock_lis_con);
					fd_max = MAX(fd_max, new_fd);

					FD_SET(new_fd, &master_fd);
					listen(sock_lis_con, BACKLOG_CONS); // Volvemos a ponernos para escuchar otra Consola

/*				} else if (fd == sock_mem){
					printf("llego algo desde memoria!\n");

					stat = recv(fd, buf, MAXMSJ, 0); // recibimos lo que nos mande
*/
				} else if (fd == sock_fs){
					printf("llego algo desde fs!\n");

					stat = recv(fd, buf, MAXMSJ, 0); // recibimos lo que nos mande

				} else if (fd == 0){ //socket del stdin
					printf("Ingreso texto por pantalla!\n");

					FD_CLR(0, &master_fd); // sacamos stdin del set a checkear, sino el ciclo queda para siempre detectando stdin...

				}

			}
		}

		//break; // TODO: ver como salir del ciclo; cuando es necesario??
	}


	// Un poco mas de limpieza antes de cerrar
	FD_ZERO(&read_fd);
	FD_ZERO(&master_fd);
//	close(sock_mem);
	close(sock_fs);
	close(sock_lis_con);
	close(sock_lis_cpu);
	liberarConfiguracionKernel(kernel);
	return stat;
}

void setupHints(struct addrinfo *hint, int fam, int sockType, int flags){

	memset(hint, 0, sizeof *hint);
	hint->ai_family = fam;
	hint->ai_socktype = sockType;
	hint->ai_flags = flags;
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

	stat = connect(sock_dest, destInfo->ai_addr, destInfo->ai_addrlen); // llamada bloqueante
	freeaddrinfo(destInfo);

	if (sock_dest < 0){
		printf("Error al tratar de conectar con Kernel!");
		return FALLO_CONEXION;
	}

	return sock_dest;
}


tKernel *getConfigKernel(char* ruta){

	printf("Ruta del archivo de configuracion: %s\n", ruta);
	tKernel *kernel = malloc(sizeof(tKernel));

	kernel->algoritmo   = malloc(MAXALGO * sizeof kernel->algoritmo);
	kernel->ip_fs       = malloc(MAX_IP_LEN * sizeof kernel->ip_fs);
	kernel->ip_memoria  = malloc(MAX_IP_LEN * sizeof kernel->ip_memoria);
	kernel->puerto_cpu  = malloc(MAX_PORT_LEN * sizeof kernel->puerto_cpu);
	kernel->puerto_prog = malloc(MAX_PORT_LEN * sizeof kernel->puerto_prog);
	kernel->puerto_memoria = malloc(MAX_PORT_LEN * sizeof kernel->puerto_memoria);
	kernel->puerto_fs   = malloc(MAX_PORT_LEN * sizeof kernel->puerto_fs);

	t_config *kernelConfig = config_create(ruta);

	strcpy(kernel->algoritmo,   config_get_string_value(kernelConfig, "ALGORITMO"));
	strcpy(kernel->ip_fs,       config_get_string_value(kernelConfig, "IP_FS"));
	strcpy(kernel->ip_memoria,  config_get_string_value(kernelConfig, "IP_MEMORIA"));
	strcpy(kernel->puerto_prog, config_get_string_value(kernelConfig, "PUERTO_PROG"));
	strcpy(kernel->puerto_cpu,  config_get_string_value(kernelConfig, "PUERTO_CPU"));
	strcpy(kernel->puerto_memoria, config_get_string_value(kernelConfig, "PUERTO_MEMORIA"));
	strcpy(kernel->puerto_fs,   config_get_string_value(kernelConfig, "PUERTO_FS"));

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
