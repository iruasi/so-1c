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

#define FALLO_GRAL -21
#define FALLO_CONFIGURACION -22
#define FALLO_RECV -23

void setupHints(struct addrinfo *hint, int address_family, int socket_type, int flags);
int setSocketComm(char *puerto_de_comunicacion);
int makeListenSock(char *port_listen);
int makeCommSock(int socket_in);

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
	int fd_max;

	struct sockaddr_in clientAddr;
	socklen_t clientSize = sizeof(clientAddr);

	tKernel *kernel = getConfigKernel(argv[1]);
	mostrarConfiguracion(kernel);


	// A esta altura ya tendraimos al Kernel conectado con una Memoria y un FS
	// No pueden usarse numeros arbitrarios para simularlos porque sino select() retorna error

	//int supuesto_socket_memoria = algun_int;
	//int supuesto_socket_filesys = algun_otro_int;


	fd_set read_fd, master_fd;
	FD_ZERO(&read_fd);
	FD_ZERO(&master_fd);

	// Agregamos Mem, FS y stdin al set de read
	//FD_SET(sock_mem_mock, &master_fd);
	//FD_SET(sock_fs_mock, &master_fd);
	FD_SET(0, &master_fd);

	// Creamos sockets para hacer listen() de CPUs o Consolas;
	// Hacemos el listen(); Y lo agregamos al set read_socks
	int sock_lis_cpu = makeListenSock(kernel->puerto_cpu);
	listen(sock_lis_cpu, BACKLOG_CPU);
	FD_SET(sock_lis_cpu, &master_fd);

	int sock_lis_con = makeListenSock(kernel->puerto_prog);
	listen(sock_lis_con, BACKLOG_CONS);
	FD_SET(sock_lis_con, &master_fd);

	// TODO: implementar funcion que retorne el verdadero fd_max
	fd_max = (sock_lis_con > sock_lis_cpu)? sock_lis_con : sock_lis_cpu;

	// Para el select() vamos a estar pasando NULL por timeout, recomendacion de la catedra
	while (1){
		read_fd = master_fd;

		ready_fds = select(fd_max + 1, &read_fd, NULL, NULL, NULL);
		if (ready_fds == -1)
			return -26; // despues vemos de hacerlo lindo...

		printf("lele");
		for (fd = 0; fd < fd_max; ++fd){

			if (FD_ISSET(fd, &read_fd)){
				printf("Aca hay uno! el fd es: %d\n", fd);

				// Para luego distinguir un recv() entre diferentes procesos, debemos definir un t_Package
				// este t_Package tendria un identificador, y un mensaje. Ambos de un size maximo predeterminado.
				if (fd == sock_lis_cpu){
					printf("es de cpu!\n");
					new_fd = makeCommSock(sock_lis_cpu);
					FD_SET(new_fd, &master_fd);

					listen(sock_lis_cpu, BACKLOG_CPU);

				} else if (fd == sock_lis_con){
					printf("es de consola!\n");
					new_fd = makeCommSock(sock_lis_con);
					FD_SET(new_fd, &master_fd);

					listen(sock_lis_con, BACKLOG_CONS); // Volvemos a ponernos para escuchar otra consola

				} else if (fd == 30){ //sock_mem_mock
					printf("es de memoria!\n");
					// aca deberiamos hacer algo especifico para memoria, ej algunos send/recv

				} else if (fd == 40){ //sock_fs_mock
					printf("es de fs!\n");
					// aca deberiamos hacer algo especifico para filesystem, ej algunos send/recv

				} else if (fd == 0){ //socket del stdin

					printf("Ingreso texto por pantalla!\n");
					FD_CLR(0, &master_fd); // sacamos stdin del set a checkear, sino el ciclo queda para siempre detectando stdin...

				}

			}
		}

		//break; // TODO: ver como salir del ciclo; cuando es necesario??
	}

/* De momento dejo comentada esta seccion, que ya no deberia utilizarse
 * Tal vez sirva para algo, pero no deberiamos depender de este bloque de codigo...

	// Creamos sockets listos para send/recv para cada proceso
	int fd_cons = setSocketComm(kernel->puerto_prog);
	int fd_cpu  = setSocketComm(kernel->puerto_cpu);
	int fd_mem  = setSocketComm(kernel->puerto_memoria);
	int fd_fs   = setSocketComm(kernel->puerto_fs);

	while ((stat = recv(fd_cons, buf, MAXMSJ, 0)) > 0){

		printf("%s\n", buf);

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
*/

	// Un poco mas de limpieza antes de cerrar
	FD_ZERO(&read_fd);
	FD_ZERO(&master_fd);
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

/* crea un socket y lo bindea() a un puerto particular, luego retorna este socket.
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

// Esta funcion ya estaria reemplazada por makeListenSock() y makeCommSock(), que separa responsabilidades
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
