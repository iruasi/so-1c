#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/inotify.h>

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
//#include "manejadores.h"

int frames, frame_size; // para guardar datos a recibir de Memoria
int sock_mem;
int sock_fs;
tKernel *kernel;

t_list * tablaProcesos;
t_dictionary * tablaGlobal;

//sem_t haySTDIN;
sem_t eventoPlani;
sem_t codigoEnviado;

int fdInotify;

int interconectarProcesos(ker_socks *ks, const char* path);
void inotifyer(char *path);

int interconectarProcesos(ker_socks *ks, const char* path){

	int stat;

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

	tPackHeader h_esp = {.tipo_de_proceso = MEM, .tipo_de_mensaje = MEMINFO};
	if((stat = recibirInfo2ProcAProc(sock_mem, h_esp, &frames, &frame_size)) == -1){
		puts("No se recibio correctamente informacion de Memoria!");
		return FALLO_GRAL;
	}
	printf("Se trabaja una Memoria con %d frames de size %d\n", frames, frame_size);

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

	ks->fd_max = MAX(sock_fs, ks->fd_max);

	// Creamos sockets para hacer listen() de CPUs
	if ((ks->sock_lis_cpu = makeListenSock(kernel->puerto_cpu)) < 0){
		fprintf(stderr, "No se pudo crear socket para escuchar! sock_lis_cpu: %d\n", ks->sock_lis_cpu);
		return FALLO_CONEXION;
	}

	ks->fd_max = MAX(ks->sock_lis_cpu, ks->fd_max);

	// Creamos sockets para hacer listen() de Consolas
	if ((ks->sock_lis_con = makeListenSock(kernel->puerto_prog)) < 0){
		fprintf(stderr, "No se pudo crear socket para escuchar! sock_lis_con: %d\n", ks->sock_lis_con);
		return FALLO_CONEXION;
	}

	ks->fd_max = MAX(ks->sock_lis_con, ks->fd_max);

	while ((stat = listen(ks->sock_lis_cpu, BACKLOG)) == -1){
		perror("Fallo listen a socket CPUs. error");
		puts("Reintentamos...\n");
	}

	while ((stat = listen(ks->sock_lis_con, BACKLOG)) == -1){
		perror("Fallo listen a socket CPUs. error");
		puts("Reintentamos...\n");
	}

	// Al inicializar inotify este nos devuelve un descriptor de archivo
	if ((ks->sock_inotify = inotify_init()) == -1){
		perror("inotify_init. error");
		puts("Se trabaja sin actualizacion de configuracion Kernel");
	}

	// Creamos un monitor sobre un path indicando que eventos queremos escuchar
	ks->sock_watch = inotify_add_watch(ks->sock_inotify, path, IN_MODIFY);
	ks->fd_max = MAX(ks->fd_max, ks->sock_watch);

	FD_SET(sock_fs,          &ks->master);
	FD_SET(ks->sock_lis_cpu, &ks->master);
	FD_SET(ks->sock_lis_con, &ks->master);
	FD_SET(ks->sock_watch,   &ks->master);
	return 0;
}

int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

//	if (sem_init(&haySTDIN, 0, 0) == -1){
//		perror("No se pudo inicializar semaforo. error");
//		return FALLO_GRAL;
//	}
	if (sem_init(&eventoPlani, 0, 0) == -1){
		perror("No se pudo inicializar semaforo. error");
		return FALLO_GRAL;
	}
	if (sem_init(&codigoEnviado, 0, 0) == -1){
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

	ker_socks *ks = malloc(sizeof *ks);
	ks->fd_max = -1;
	int stat, ready_fds;
	int fd;

	// Creamos e inicializamos los conjuntos que retendran sockets para el select()
	fd_set read_fd;
	FD_ZERO(&read_fd);
	FD_ZERO(&ks->master);
	//FD_SET(0, &ks->master); por ahora lo deshabilitamos, no lo necesitamos

	kernel = getConfigKernel(argv[1]);
	mostrarConfiguracion(kernel);

	if (interconectarProcesos(ks, argv[1]) != 0){
		puts("Fallo en la conexion con el resto de los procesos");
		return ABORTO_KERNEL;
	}

	setupHeapStructs();
	void setupMutexes();
	setupVariablesGlobales();

	tablaGlobal = dictionary_create();
	tablaProcesos = list_create();

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

		read_fd = ks->master;

		ready_fds = select(ks->fd_max + 1, &read_fd, NULL, NULL, NULL);
		if(ready_fds == -1){
			perror("Fallo el select(). error");
			return FALLO_SELECT;
		}

		for (fd = 0; fd <= ks->fd_max; ++fd){
			if (FD_ISSET(fd, &read_fd)){

				printf("Hay un socket listo! El fd es: %d\n", fd);


				if(fd == ks->sock_watch){
					puts("El socket es de inotify");
					inotifyer(argv[1]);
				}

				// Controlamos el listen de CPU o de Consola
				if (fd == ks->sock_lis_cpu){

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

				} else if (fd == ks->sock_lis_con){

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
					clearAndClose(&fd, &ks->master);
					break;

				} else if (fd == sock_fs){


					printf("llego algo desde fs!\n\tTipo de mensaje: %d\n", header_tmp.tipo_de_mensaje);
					break;

				} else if (fd == 0) //socket del stdin
//					sem_post(&haySTDIN);

				puts("Si esta linea se imprime, es porque el header_tmp tiene algun valor rarito...");
				printf("El valor de header_tmp es: proceso %d \t mensaje: %d\n", header_tmp.tipo_de_proceso, header_tmp.tipo_de_mensaje);

			}} // aca terminan el for() y el if(FD_ISSET)
	}

	// Un poco mas de limpieza antes de cerrar

	FD_ZERO(&read_fd);
	FD_ZERO(&ks->master);
	close(sock_mem);
	close(sock_fs);
	close(ks->sock_lis_con);
	close(ks->sock_lis_cpu);
	inotify_rm_watch(ks->sock_inotify, ks->sock_watch);
	free(ks);
	liberarConfiguracionKernel(kernel);
	return stat;
}

void inotifyer(char *path){

	// El file descriptor creado por inotify, es el que recibe la información sobre los eventos ocurridos
	// para leer esta información el descriptor se lee como si fuera un archivo comun y corriente pero
	// la diferencia esta en que lo que leemos no es el contenido de un archivo sino la información
	// referente a los eventos ocurridos

	char buffer[BUF_LEN];

	int length;
	if ((length = read(fdInotify, buffer, BUF_LEN)) == -1){
		perror("read Inotify. error");
		return;
	}
	int offset = 0;

	// Luego del read, buffer es un array de n posiciones donde cada posición contiene
	// un evento ( inotify_event ) junto con el nombre de este.
	while (offset < length) {

		// El buffer es de tipo array de char, o array de bytes. Esto es porque como los
		// nombres pueden tener nombres mas cortos que 24 caracteres el tamaño va a ser menor
		// a sizeof( struct inotify_event ) + 24.
		struct inotify_event *event = (struct inotify_event *) &buffer[offset];

		// El campo "len" nos indica la longitud del tamaño del nombre
		if (event->len){
			// Dentro de "mask" tenemos el evento que ocurrio y sobre donde ocurrio
			// sea un archivo o un directorio
			if ((event->mask & IN_MODIFY) && !(event->mask & IN_ISDIR)){
				printf("The file %s was modified.\n", event->name);

				if (strncmp(event->name, "config_kernel", 13) == 0){
					puts("Actualizamos los valores de la config de kernel");
					liberarConfiguracionKernel(kernel);
					kernel = getConfigKernel(path);
					break;
				}

			} else {
				printf("Hubo una accion sobre %s que no manejamos...\n", event->name);
				break;
			}
		}
	}
}
