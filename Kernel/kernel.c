#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>

#include <commons/config.h>
#include <commons/string.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include <parser/parser.h>

#include <funcionesCompartidas/funcionesCompartidas.h>
#include <funcionesPaquetes/funcionesPaquetes.h>
#include <tiposRecursos/tiposErrores.h>
#include <tiposRecursos/tiposPaquetes.h>
#include <tiposRecursos/misc/pcb.h>

#include "defsKernel.h"
#include "capaMemoria.h"
#include "kernelConfigurators.h"
#include "auxiliaresKernel.h"
#include "planificador.h"


int MAX_ALLOC_SIZE; // con esta variable se debe comprobar que CPU no pida mas que este size de HEAP
int frames, frame_size; // para guardar datos a recibir de Memoria
int sock_mem;
tKernel *kernel;

sem_t hayProg; // semaforo estilo productor-consumidor. Se post'ea cuando entran PCBs en New o Ready
sem_t hayCPUs;
sem_t eventoPlani;
sem_t codigoEnviado;

int interconectarProcesos(int *sock_fs, int *sock_lis_con, int *sock_lis_cpu, int *fd_max, fd_set *master_fd){

	int stat;
	*fd_max = -1;

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

	if((stat = recibirInfoKerMem(sock_mem, &frames, &frame_size)) == -1){
		puts("No se recibio correctamente informacion de Memoria!");
		return FALLO_GRAL;
	}

	//*fd_max = MAX(sock_mem, *fd_max);

	// Se trata de conectar con Filesystem
	if ((*sock_fs = establecerConexion(kernel->ip_fs, kernel->puerto_fs)) < 0){
		fprintf(stderr, "No se pudo conectar con el Filesystem! sock_fs: %d\n", *sock_fs);
		return FALLO_CONEXION;
	}

	// No permitimos continuar la ejecucion hasta lograr un handshake con Filesystem
	if ((stat = handshakeCon(*sock_fs, kernel->tipo_de_proceso)) < 0){
		fprintf(stderr, "No se pudo hacer hadshake con Filesystem\n");
		return FALLO_GRAL;
	}
	printf("Se enviaron: %d bytes a FILESYSTEM\n", stat);

	*fd_max = MAX(*sock_fs, *fd_max);

	// Creamos sockets para hacer listen() de CPUs
	if ((*sock_lis_cpu = makeListenSock(kernel->puerto_cpu)) < 0){
		fprintf(stderr, "No se pudo crear socket para escuchar! sock_lis_cpu: %d\n", *sock_lis_cpu);
		return FALLO_CONEXION;
	}

	*fd_max = MAX(*sock_lis_cpu, *fd_max);

	// Creamos sockets para hacer listen() de Consolas
	if ((*sock_lis_con = makeListenSock(kernel->puerto_prog)) < 0){
		fprintf(stderr, "No se pudo crear socket para escuchar! sock_lis_con: %d\n", *sock_lis_con);
		return FALLO_CONEXION;
	}

	*fd_max = MAX(*sock_lis_con, *fd_max);

	// Se agregan memoria, fs, listen_cpu, listen_consola y stdin al set master
	//FD_SET(sock_mem,      master_fd);
	FD_SET(*sock_fs,      master_fd);
	FD_SET(*sock_lis_cpu, master_fd);
	FD_SET(*sock_lis_con, master_fd);
	FD_SET(0,             master_fd);

	while ((stat = listen(*sock_lis_cpu, BACKLOG)) == -1){
		perror("Fallo listen a socket CPUs. error");
		puts("Reintentamos...\n");
	}

	while ((stat = listen(*sock_lis_con, BACKLOG)) == -1){
		perror("Fallo listen a socket CPUs. error");
		puts("Reintentamos...\n");
	}

	return 0;
}

int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	int stat, ready_fds;
	int fd;
	int fd_max;
	int sock_fs, sock_lis_cpu, sock_lis_con;

	if ((stat = sem_init(&hayProg, 0, 0)) == -1){
		perror("No se pudo inicializar semaforo. error");
		return FALLO_GRAL;
	}
	if ((stat = sem_init(&hayCPUs, 0, 0)) == -1){
		perror("No se pudo inicializar semaforo. error");
		return FALLO_GRAL;
	}
	if ((stat = sem_init(&eventoPlani, 0, 0)) == -1){
		perror("No se pudo inicializar semaforo. error");
		return FALLO_GRAL;
	}
	if ((stat = sem_init(&codigoEnviado, 0, 0)) == -1){
		perror("No se pudo inicializar semaforo. error");
		return FALLO_GRAL;
	}

	pthread_attr_t attr_ondemand;
	pthread_attr_init(&attr_ondemand);
	pthread_attr_setdetachstate(&attr_ondemand, PTHREAD_CREATE_DETACHED);

	pthread_t consolaKernel_thread;
	pthread_t planif_thread;
	pthread_t mem_thread;
//	pthread_t fs_thread;

	// Creamos e inicializamos los conjuntos que retendran sockets para el select()
	fd_set read_fd, master_fd;
	FD_ZERO(&read_fd);
	FD_ZERO(&master_fd);

	kernel = getConfigKernel(argv[1]);
	mostrarConfiguracion(kernel);

	if ((stat = interconectarProcesos(&sock_fs, &sock_lis_con, &sock_lis_cpu, &fd_max, &master_fd)) != 0){
		puts("Fallo en la conexion con el resto de los procesos");
		return ABORTO_KERNEL;
	}

	setupHeapStructs();
	setupVariablesGlobales();

	if( pthread_create(&planif_thread, NULL, (void*) setupPlanificador, NULL) < 0){
		perror("no pudo crear hilo. error");
		return FALLO_GRAL;
	}

	if( pthread_create(&consolaKernel_thread, NULL, (void*) consolaKernel, NULL) < 0){
		perror("no pudo crear hilo. error");
		return FALLO_GRAL;
	}

	if( pthread_create(&mem_thread, NULL, (void*) mem_manejador, (void*) &sock_mem) < 0){
		perror("no pudo crear hilo. error");
		return FALLO_GRAL;
	}

	tPackHeader header_tmp;
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

				int *sock_cpu = malloc(sizeof(int));
				if ((*sock_cpu = makeCommSock(fd)) < 0)
					break; // Fallo y no se conecto el CPU

				t_RelCC* cpu_i = malloc(sizeof *cpu_i); cpu_i->con = malloc(sizeof *cpu_i->con);
				cpu_i->cpu.fd_cpu = *sock_cpu; cpu_i->cpu.pid = PID_IDLE;

				pthread_t cpu_thread;
				if( pthread_create(&cpu_thread, &attr_ondemand, (void*) cpu_manejador, (void*) cpu_i) < 0){
					perror("no pudo crear hilo. error");
					return FALLO_GRAL;
				}

				break;

			} else if (fd == sock_lis_con){

				int *sock_con = malloc(sizeof(int));
				if ((*sock_con = makeCommSock(fd)) < 0)
					break; // Fallo y no se conecto el Programa

				t_RelCC* con_i = malloc(sizeof *con_i); con_i->con = malloc(sizeof *con_i->con);
				con_i->con->fd_con = *sock_con;

				pthread_t con_thread;
				if( pthread_create(&con_thread, &attr_ondemand, (void*) cons_manejador, (void*) con_i) < 0){
					perror("no pudo crear hilo. error");
					return FALLO_GRAL;
				}

				break;
			}

			// Como no es un listen, recibimos el header de lo que llego
			if ((stat = recv(fd, &header_tmp, HEAD_SIZE, 0)) == -1){
				perror("Error en recv() de algun socket. error");
				fprintf(stderr, "El socket asociado al fallo es: %d\n", fd);
				break;

			} else if (stat == 0){
				printf("Se desconecto el socket %d\nLo sacamos del set listen...\n", fd);
				clearAndClose(&fd, &master_fd);
				break;

			} else if (fd == sock_fs){

				printf("llego algo desde fs!\n\tTipo de mensaje: %d\n", header_tmp.tipo_de_mensaje);
				break;

			} else if (fd == 0){ //socket del stdin
				printf("Ingreso texto por pantalla!\nCerramos Kernel!\n");
				goto limpieza; // nos vamos del ciclo infinito...
			}

			puts("Si esta linea se imprime, es porque el header_tmp tiene algun valor rarito...");
			printf("El valor de header_tmp es: proceso %d \t mensaje: %d\n", header_tmp.tipo_de_proceso, header_tmp.tipo_de_mensaje);

		}} // aca terminan el for() y el if(FD_ISSET)
	}

limpieza:
	// Un poco mas de limpieza antes de cerrar

	FD_ZERO(&read_fd);
	FD_ZERO(&master_fd);
	close(sock_mem);
	close(sock_fs);
	close(sock_lis_con);
	close(sock_lis_cpu);
	liberarConfiguracionKernel(kernel);
	return stat;
}
