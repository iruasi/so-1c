#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>

#include <commons/config.h>
#include <commons/string.h>
#include <commons/log.h>
#include <commons/collections/list.h>

#include <funcionesCompartidas/funcionesCompartidas.h>
#include <funcionesPaquetes/funcionesPaquetes.h>
#include <tiposRecursos/tiposErrores.h>
#include <tiposRecursos/tiposPaquetes.h>
#include <tiposRecursos/misc/pcb.h>

#include "kernelConfigurators.h"
#include "auxiliaresKernel.h"
#include "planificador.h"

#ifndef HEAD_SIZE
#define HEAD_SIZE 8
#endif

#define BACKLOG 20

/* MAX(A, B) es un macro que devuelve el mayor entre dos argumentos,
 * lo usamos para actualizar el maximo socket existente, a medida que se crean otros nuevos
 */
#define MAX(A, B) ((A) > (B) ? (A) : (B))

void cpu_manejador(int sock_cpu, tMensaje msj);

int sock_cpu;
int main(int argc, char* argv[]){
	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	int stat, ready_fds;
	int fd, new_fd;
	int fd_max = -1;
	int sock_fs, sock_mem;
	int sock_lis_cpu, sock_lis_con;
	int frames, frame_size; // para guardar datos a recibir de Memoria

	// Creamos e inicializamos los conjuntos que retendran sockets para el select()
	fd_set read_fd, master_fd;
	FD_ZERO(&read_fd);
	FD_ZERO(&master_fd);

	tKernel *kernel = getConfigKernel(argv[1]);
	mostrarConfiguracion(kernel);

	// Se trata de conectar con Memoria
	if ((sock_mem = establecerConexion(kernel->ip_memoria, kernel->puerto_memoria)) < 0){
		fprintf(stderr, "No se pudo conectar con la Memoria! sock_mem: %d\n", sock_mem);
		return FALLO_CONEXION;
	}

	// No permitimos continuar la ejecucion hasta lograr un handshake con Memoria
	if ((stat = handshakeCon(sock_mem, kernel->tipo_de_proceso)) < 0){
		fprintf(stderr, "No se pudo hacer hadshake con Memoria\n");
		return FALLO_GRAL;
	}
	printf("Se enviaron: %d bytes a MEMORIA\n", stat);

	fd_max = MAX(sock_mem, fd_max);

	// Se trata de conectar con Filesystem
	if ((sock_fs = establecerConexion(kernel->ip_fs, kernel->puerto_fs)) < 0){
		fprintf(stderr, "No se pudo conectar con el Filesystem! sock_fs: %d\n", sock_fs);
		return FALLO_CONEXION;
	}

	// No permitimos continuar la ejecucion hasta lograr un handshake con Filesystem
	if ((stat = handshakeCon(sock_fs, kernel->tipo_de_proceso)) < 0){
		fprintf(stderr, "No se pudo hacer hadshake con Filesystem\n");
		return FALLO_GRAL;
	}
	printf("Se enviaron: %d bytes a FILESYSTEM\n", stat);

	fd_max = MAX(sock_fs, fd_max);

	// Creamos sockets para hacer listen() de CPUs
	if ((sock_lis_cpu = makeListenSock(kernel->puerto_cpu)) < 0){
		fprintf(stderr, "No se pudo crear socket para escuchar! sock_lis_cpu: %d\n", sock_lis_cpu);
		return FALLO_CONEXION;
	}

	fd_max = MAX(sock_lis_cpu, fd_max);

	// Creamos sockets para hacer listen() de Consolas
	if ((sock_lis_con = makeListenSock(kernel->puerto_prog)) < 0){
		fprintf(stderr, "No se pudo crear socket para escuchar! sock_lis_con: %d\n", sock_lis_con);
		return FALLO_CONEXION;
	}

	fd_max = MAX(sock_lis_con, fd_max);

	// Se agregan memoria, fs, listen_cpu, listen_consola y stdin al set master
	FD_SET(sock_mem, &master_fd);
	FD_SET(sock_fs, &master_fd);
	FD_SET(sock_lis_cpu, &master_fd);
	FD_SET(sock_lis_con, &master_fd);
	FD_SET(0, &master_fd);

	while ((stat = listen(sock_lis_cpu, BACKLOG)) == -1){
		perror("Fallo listen a socket CPUs. error");
		puts("Reintentamos...\n");
	}

	while ((stat = listen(sock_lis_con, BACKLOG)) == -1){
		perror("Fallo listen a socket CPUs. error");
		puts("Reintentamos...\n");
	}


	tPackHeader *header_tmp = malloc(HEAD_SIZE); // para almacenar cada recv
	while (1){

		read_fd = master_fd;

		ready_fds = select(fd_max + 1, &read_fd, NULL, NULL, NULL);
		if(ready_fds == -1){
			perror("Fallo el select(). error");
			return FALLO_SELECT;
		}

		for (fd = 0; fd <= fd_max; ++fd){
		if (FD_ISSET(fd, &read_fd)){

			printf("Hay un socket listo! El fd es: %d\n", fd);

			// Controlamos el listen de CPU o de Consola
			if (fd == sock_lis_cpu){
				new_fd = handleNewListened(fd, &master_fd);
				if (new_fd < 0){
					perror("Fallo en manejar un listen. error");
					return FALLO_CONEXION;
				}

				fd_max = MAX(new_fd, fd_max);


				break;

			} else if (fd == sock_lis_con){

				new_fd = handleNewListened(fd, &master_fd);
				if (new_fd < 0){
					perror("Fallo en manejar un listen. error");
					return FALLO_CONEXION;
				}

				fd_max = MAX(new_fd, fd_max);
				break;
			}

			// Como no es un listen, recibimos el header de lo que llego
			if ((stat = recv(fd, header_tmp, HEAD_SIZE, 0)) == -1){
				perror("Error en recv() de algun socket. error");
				break;

			} else if (stat == 0){
				printf("Se desconecto el socket %d\nLo sacamos del set listen...\n", fd);
				clearAndClose(&fd, &master_fd);
				break;
			}

			// Se recibio un header sin conflictos, procedemos con el flujo..
			if (header_tmp->tipo_de_mensaje == SRC_CODE){
				puts("Consola quiere iniciar un programa");

				int src_size;
				puts("Entremos a passSrcCodeFromRecv");
				passSrcCodeFromRecv(header_tmp, fd, sock_mem, &src_size);

				tPCB *new_pcb = nuevoPCB((int) ceil((float) src_size / frame_size) + kernel->stack_size);
				encolarPrograma(new_pcb, fd);

				puts("Listo!");
				break;
			}

			if (fd == sock_mem){
				printf("Llego algo desde memoria!\n\tTipo de mensaje: %d\n", header_tmp->tipo_de_mensaje);

				if (header_tmp->tipo_de_mensaje != MEMINFO)
					break;


				if((stat = recibirInfoMem(fd, &frames, &frame_size)) == -1){
					puts("No se recibio correctamente informacion de Memoria!");
					return FALLO_GRAL;
				}

				break;

			} else if (fd == sock_fs){
				printf("llego algo desde fs!\n\tTipo de mensaje: %d\n", header_tmp->tipo_de_mensaje);
				break;

			} else if (fd == 0){ //socket del stdin
				printf("Ingreso texto por pantalla!\nCerramos Kernel!\n");
				goto limpieza; // nos vamos del ciclo infinito...
			}

			if (header_tmp->tipo_de_proceso == CON){
				printf("Llego algo desde Consola!\n\tTipo de mensaje: %d\n", header_tmp->tipo_de_mensaje);
				break;
			}

			if (header_tmp->tipo_de_proceso == CPU){
				printf("Llego algo desde CPU!\n\tTipo de mensaje: %d\n", header_tmp->tipo_de_mensaje);
				cpu_manejador(fd, header_tmp->tipo_de_mensaje);
				break;
			}

			puts("Si esta linea se imprime, es porque el header_tmp tiene algun valor rarito...");
			printf("El valor de header_tmp es: proceso %d \t mensaje: %d", header_tmp->tipo_de_proceso, header_tmp->tipo_de_mensaje);

		}} // aca terminan el for() y el if(FD_ISSET)
	}

limpieza:
	// Un poco mas de limpieza antes de cerrar

	free(header_tmp);

	FD_ZERO(&read_fd);
	FD_ZERO(&master_fd);
	close(sock_mem);
	close(sock_fs);
	close(sock_lis_con);
	close(sock_lis_cpu);

	liberarConfiguracionKernel(kernel);
	return stat;
}

void cpu_manejador(int sock_cpu, tMensaje msj){

	switch(msj){
	case S_WAIT:
		puts("Funcion wait!");
		break;
	case S_SIGNAL:
		puts("Funcion signal!");
		break;
	case LIBERAR:
		puts("Funcion liberar!");
		break;
	case ABRIR:
		break;
	case BORRAR:
		break;
	case CERRAR:
		break;
	case MOVERCURSOR:
		break;
	case ESCRIBIR:
		break;
	case LEER:
		break;
	case RESERVAR:
		break;
	default:
		puts("Funcion no reconocida!");
		break;
	}
}
