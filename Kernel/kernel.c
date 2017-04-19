#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h> // biblioteca que tiene el close()

#include <commons/config.h>
#include <commons/string.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include "../funcionesComunes.h"
#include "kernelConfigurators.h"

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
