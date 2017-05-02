#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h> // biblioteca que tiene el close()

#include <commons/config.h>
#include <commons/string.h>
#include <commons/log.h>
#include <commons/collections/list.h>

#include "../Compartidas/funcionesCompartidas.c"
#include "../Compartidas/tiposErrores.h"
#include "kernelConfigurators.h"

#define BACKLOG 20 // cantidad de procesos que pueden enfilar en un solo listen

#define MAX_IP_LEN 16   // aaa.bbb.ccc.ddd -> son 15 caracteres, 16 contando un '\0'
#define MAX_PORT_LEN 6  // 65535 -> 5 digitos, 6 contando un '\0'
#define MAXMSJ 100

#define MAXCPUS 10

/* MAX(A, B) es un macro que devuelve el mayor entre dos argumentos,
 * lo usamos para actualizar el maximo socket existente, a medida que se crean otros nuevos
 */
#define MAX(A, B) ((A) > (B) ? (A) : (B))


int handleNewListened(int socket_listen, fd_set *setFD);
int sendReceived(int status, char *buffer, int *sockets, int total_sockets);
void clearAndClose(int *fd, fd_set *setFD);


int main(int argc, char* argv[]){
	if(argc!=2){
			printf("Error en la cantidad de parametros\n");
			return EXIT_FAILURE;
	}

	char *buf = malloc(MAXMSJ);
	int stat, ready_fds;
	int fd, new_fd;
	int sock_fs, sock_mem;
	int *socksToSend = malloc((2 * MAXCPUS ) * sizeof *socksToSend); // aca almacenamos todos los sockets a los que queremos enviar
	int ctrlSend = 0;
	int sock_lis_cpu, sock_lis_con;
	int bytes_sent;
	int fd_max = -1;

	// Creamos e inicializamos los conjuntos que retendran sockets para el select()
	fd_set read_fd, master_fd;
	FD_ZERO(&read_fd);
	FD_ZERO(&master_fd);

	tKernel *kernel = getConfigKernel(argv[1]);
	mostrarConfiguracion(kernel);
	strcpy(buf, "Hola soy Kernel");

	strcpy(buf, "Hola soy Kernel");

		// Se trata de conectar con Memoria
		if ((sock_mem = establecerConexion(kernel->ip_memoria, kernel->puerto_memoria)) < 0){
			printf("Error fatal! No se pudo conectar con la Memoria! sock_mem: %d\n", sock_mem);
			return FALLO_CONEXION;
		}

		//paso estructura dinamica
		t_Package package;
		package.tipo_de_proceso = kernel->tipo_de_proceso;
		package.tipo_de_mensaje = 1;
		//package.archivo_a_enviar = buf;
		//package.message_long = strlen(package.message)+1;
		package.total_size = sizeof(package.tipo_de_proceso)+sizeof(package.tipo_de_mensaje)+sizeof(package.total_size);

		char *serializedPackage;
		serializedPackage = serializarOperandos(&package);

		send(sock_mem, serializedPackage, package.total_size, 0);
		//fin envio estructura dinamica
		// No permitimos continuar la ejecucion hasta lograr un handshake con Memoria
		/*while ((bytes_sent = send(sock_mem, buf, strlen(buf), 0)) < 0)
			;
		printf("Se enviaron: %d bytes a MEMORIA\n", bytes_sent);
	*/
		fd_max = MAX(sock_mem, fd_max);

	// Se trata de conectar con Filesystem
	if ((sock_fs  = establecerConexion(kernel->ip_fs, kernel->puerto_fs)) < 0){
		printf("Error fatal! No se pudo conectar con el Filesystem! sock_fs: %d\n", sock_fs);
		return FALLO_CONEXION;
	}

	// No permitimos continuar la ejecucion hasta lograr un handshake con Filesystem
	while ((bytes_sent = send(sock_fs, buf, strlen(buf), 0)) < 0)
		;
	printf("Se enviaron: %d bytes a FILESYSTEM\n", bytes_sent);

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
	FD_SET(sock_mem, &master_fd);
	FD_SET(sock_fs, &master_fd);
	FD_SET(sock_lis_cpu, &master_fd);
	FD_SET(sock_lis_con, &master_fd);
	FD_SET(0, &master_fd);


	listen(sock_lis_cpu, BACKLOG);
	listen(sock_lis_con, BACKLOG);

	// Cargamos los valores de los sockets a enviar manualmente...
	socksToSend[0] = sock_fs;
	ctrlSend = 1;


	while (1){
		read_fd = master_fd;

		ready_fds = select(fd_max + 1, &read_fd, NULL, NULL, NULL);
		if (ready_fds == -1)
			return FALLO_SELECT; // despues vemos de hacerlo lindo...


		for (fd = 0; fd <= fd_max; ++fd){

			if (FD_ISSET(fd, &read_fd)){
				printf("Aca hay uno! el fd es: %d\n", fd);

				// Para luego distinguir un recv() entre diferentes procesos, debemos definir un t_Package
				// este t_Package tendria un identificador, y un mensaje.
				if (fd == sock_lis_cpu){
					printf("es de listen_cpu!\n");

					new_fd = handleNewListened(sock_lis_cpu, &master_fd);

					if(ctrlSend < (MAXCPUS*2 - 1)){
						socksToSend[ctrlSend] = new_fd;
						ctrlSend++;
						fd_max = MAX(fd_max, new_fd);

					} else {
						printf("Se llego al tope de CPUS permitidos!\n No aceptamos esta conexion...\n");
						close(new_fd);
					}

				} else if (fd == sock_lis_con){
					printf("es de listen_consola!\n");

					new_fd = handleNewListened(sock_lis_con, &master_fd);
					fd_max = MAX(fd_max, new_fd);

				} else if (fd == sock_mem){
					printf("llego algo desde memoria!\n");

					stat = recv(fd, buf, MAXMSJ, 0); // recibimos lo que nos mande

					/*
					if ((stat == 0) || (stat == -1)){ // se va MEM
						printf("recv del Memoria retorno con: %d\nLo sacamos del set master\n", stat);
						clearAndClose(&fd, &master_fd);
					}

					puts("Perdimos conexion con Memoria! Entremos en Panico!\n");
					return ABORTO_MEMORIA;
					 */
				} else if (fd == sock_fs){
					printf("llego algo desde fs!\n");

					stat = recv(fd, buf, MAXMSJ, 0); // recibimos lo que nos mande, aunque esto no deba suceder todavia
					/*
					if ((stat == 0) || (stat == -1)){ // se va FS
						printf("recv del Filesystem retorno con: %d\nLo sacamos del set master\n", stat);
						clearAndClose(&fd, &master_fd);
					}
					*/
					puts("Perdimos conexion con Filesystem! Entremos en Panico!\n");
					return ABORTO_FILESYSTEM;

					//sendReceived(stat, &fd, &master_fd);

				} else if (fd == 0){ //socket del stdin
					printf("Ingreso texto por pantalla!\nCerramos Kernel!\n");
					goto limpieza; // nos vamos del ciclo infinito...

				} else { // es otro tipo de socket

					if ((stat = recv(fd, buf, MAXMSJ, 0)) < 0){ // recibimos lo que nos mande
						printf("Error en recepcion de datos!\n Cierro el socket!");
						clearAndClose(&fd, &master_fd);
						continue;
					} else if (stat == 0){
						clearAndClose(&fd, &master_fd);
						continue;
					}
					printf("Mandemos algo...\n");
					stat = sendReceived(stat, buf, socksToSend, ctrlSend);
					printf("stat: %d\n", stat);
				}

			}
		}

		//break; // TODO: ver como salir del ciclo; cuando es necesario??
	}

limpieza:
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

/* Reenvia un mensaje a todos los sockets de una lista dada
 */
int sendReceived(int stat, char *buf, int *socks, int socks_tot){

	int i;
	for(i = 0; i < socks_tot; ++i){
		printf("Estoy mandando al socket %d!\n", socks[i]);
		stat = send(socks[i], buf, MAXMSJ, 0);
			if (stat < 0)
				return -28;
	}

	clearBuffer(buf, MAXMSJ);
	return stat;
}

/* borra un socket del set indicado y lo cierra
 */
void clearAndClose(int *fd, fd_set *setFD){
	FD_CLR(*fd, setFD);
	close(*fd);
}

/* Atiende una conexion entrante, la agrega al set de relevancia, y vuelve a escuhar mas conexiones;
 * retorna el nuevo socket producido
 */
int handleNewListened(int sock_listen, fd_set *setFD){

	int new_fd = makeCommSock(sock_listen);
	FD_SET(new_fd, setFD);
	listen(sock_listen, BACKLOG);

	return new_fd;
}
char* serializarOperandos(t_Package *package){

	char *serializedPackage = malloc(package->total_size);
	int offset = 0;
	int size_to_send;


	size_to_send =  sizeof(package->tipo_de_proceso);
	memcpy(serializedPackage + offset, &(package->tipo_de_proceso), size_to_send);
	offset += size_to_send;


	size_to_send =  sizeof(package->tipo_de_mensaje);
	memcpy(serializedPackage + offset, &(package->tipo_de_mensaje), size_to_send);
	offset += size_to_send;

	/*size_to_send =  sizeof(package->message_long);
	memcpy(serializedPackage + offset, &(package->message_long), size_to_send);
	offset += size_to_send;

	size_to_send =  package->message_long;
	memcpy(serializedPackage + offset, package->message, size_to_send);*/

	return serializedPackage;
}
int recieve_and_deserialize(t_Package *package, int socketCliente){

	int status;
	int buffer_size;
	char *buffer = malloc(buffer_size = sizeof(uint32_t));
	clearBuffer(buffer,buffer_size);

	uint32_t tipo_de_proceso;
	status = recv(socketCliente, buffer, sizeof(package->tipo_de_proceso), 0);
	memcpy(&(tipo_de_proceso), buffer, buffer_size);
	if (!status) return 0;

	uint32_t tipo_de_mensaje;
	status = recv(socketCliente, buffer, sizeof(package->tipo_de_mensaje), 0);
	memcpy(&(tipo_de_mensaje), buffer, buffer_size);
	if (!status) return 0;


	uint32_t archivo_size;
	status = recv(socketCliente, buffer, sizeof(package->archivo_size), 0);
	memcpy(&(archivo_size), buffer, buffer_size);
	if (!status) return 0;

	void* algo;
	status = recv(socketCliente, buffer, archivo_size, 0);
	memcpy(&(algo), buffer, buffer_size);

	//status = recv(socketCliente, package->message, message_long, 0);
	//if (!status) return 0;

	printf("%d %d %s",tipo_de_proceso,tipo_de_mensaje,algo);

	free(buffer);

	return status;
}
