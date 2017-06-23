#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <netdb.h>
#include <commons/config.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h> //inet_addr
#include <unistd.h>    //write
#include <errno.h>

#include <tiposRecursos/tiposErrores.h>
#include <tiposRecursos/tiposPaquetes.h>
#include <funcionesPaquetes/funcionesPaquetes.h>
#include <funcionesCompartidas/funcionesCompartidas.h>

#include "apiMemoria.h"
#include "manejadoresMem.h"
#include "manejadoresCache.h"
#include "memoriaConfigurators.h"
#include "structsMem.h"
#include "flujosMemoria.h"

#define BACKLOG 20

void* kernel_handler(void *sock_ker);
void* cpu_handler(void *sock_cpu);

tMemoria *memoria;          // configuracion del proceso Memoria
char *MEM_FIS;              // Memoria Fisica
char *CACHE;                // memoria CACHE
tCacheEntrada *CACHE_lines; // vector de lineas a CACHE
int  *CACHE_accs;           // vector de accesos hechos a CACHE

int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	int stat;

	memoria = getConfigMemoria(argv[1]);
	mostrarConfiguracion(memoria);

	if ((stat = setupMemoria()) != 0)
		return ABORTO_MEMORIA;

/*  Esto bloquea la ejecucion del resto de la Memoria... queda comentado de momento
	pthread_t consola_mem;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&consola_mem, &attr, consolaUsuario(), NULL);
*/

	pthread_t kern_thread;
	bool kernExists = false;
	int sock_entrada , client_sock , clientSize;

	struct sockaddr_in client;
	clientSize = sizeof client;

	if ((sock_entrada = makeListenSock(memoria->puerto_entrada)) < 0){
		fprintf(stderr, "No se pudo crear un socket de listen. fallo: %d", sock_entrada);
		return FALLO_GRAL;
	}
	//Listen
	if ((stat = listen(sock_entrada , BACKLOG)) == -1){
		perror("No se pudo hacer listen del socket. error");
		return FALLO_GRAL;
	}

	//acepta y escucha comunicaciones
	tPackHeader head;
	puts("esperando comunicaciones entrantes...");
	while((client_sock = accept(sock_entrada, (struct sockaddr*) &client, (socklen_t*) &clientSize)) != -1){
		puts("Conexion aceptada");

		pthread_t sniffer_thread;

		if ((stat = recv(client_sock, &head, HEAD_SIZE, 0)) < 0){
			perror("Error en la recepcion de handshake. error");
			return FALLO_RECV;
		}

		switch(head.tipo_de_proceso){
		case KER:

			if (!kernExists){
				int *sock_ker = malloc(sizeof(int));
				*sock_ker  = client_sock;
				kernExists = true;

				if ((stat = contestarMemoriaKernel(memoria->marco_size, memoria->marcos, *sock_ker)) == -1){
					puts("No se pudo enviar la informacion relevante a Kernel!");
					return FALLO_GRAL;
				}

				if( pthread_create(&kern_thread, NULL, (void*) kernel_handler, (void*) sock_ker) < 0){
					perror("No pudo crear hilo. error");
					return FALLO_GRAL;
				}

			} else {
				fprintf(stderr, "Se trato de conectar otro Kernel. Ignoramos el paquete...\n");
			}

			break;

		case CPU:
			if( pthread_create(&sniffer_thread, NULL, (void*) cpu_handler, (void*) &client_sock) < 0){
				perror("no pudo crear hilo. error");
				return FALLO_GRAL;
			}
			break;

		default:
			puts("Trato de conectarse algo que no era ni Kernel ni CPU!");
			printf("El tipo de proceso y mensaje son: %d y %d\n", head.tipo_de_proceso, head.tipo_de_mensaje);
			printf("Se recibio esto del socket: %d\n", client_sock);
			return CONEX_INVAL;
		}
	}

	// Si salio del ciclo es porque fallo el accept()
	perror("Fallo el accept(). error");

	liberarConfiguracionMemoria();
	return 0;
}


/* dado el socket de Kernel, maneja las acciones que de este reciba
 */
void* kernel_handler(void *sock_kernel){

	int *sock_ker = (int *) sock_kernel;
	int stat, new_page;
	int pid, pageCount;

	tPackHeader *head = calloc(1, HEAD_SIZE);

	printf("Esperamos que lleguen cosas del socket Kernel: %d\n", *sock_ker);

	do {

		switch(head->tipo_de_mensaje){
		case INI_PROG:
			puts("Kernel quiere inicializar un programa.");

			if ((stat = recv(*sock_ker, &pid, sizeof(int), 0)) == -1){
				perror("Fallo recepcion pid del Kernel. error");
				break;
			}

			if ((stat = recv(*sock_ker, &pageCount, sizeof(int), 0)) == -1){
				perror("Fallo recepcion pageCount del Kernel. error");
				break;
			}

			if ((stat = inicializarPrograma(pid, pageCount)) != 0){
				puts("No se pudo inicializar el programa. Se aborta el programa.");
				abortar(pid);
			}

			puts("Listo.");
			break;

		case ALMAC_BYTES:
			puts("Se recibio peticion de almacenamiento");

			if ((stat = manejarAlmacenamientoBytes(*sock_ker)) != 0)
				fprintf(stderr, "Fallo el manejo de la Almacenamiento de Byes. status: %d\n", stat);

			break;

		case ASIGN_PAG:
			puts("Kernel quiere asignar paginas!");

			recv(*sock_ker, &pid, sizeof pid, 0);
			recv(*sock_ker, &pageCount, sizeof pageCount, 0);

			if ((new_page = asignarPaginas(pid, pageCount)) != 0){
				fprintf(stderr, "No se pudieron asignar %d paginas al proceso %d\n", pageCount, pid);
				//return new_page;
			}

			//responderAsignacion(*sock_ker, new_page); todo: send(sock_ker, ASIGN_PAG_SUCCESS ...);

			break;

		case FIN_PROG:

			recv(*sock_ker, &pid, sizeof pid, 0);
			// TODO: desalojarDatosPrograma(pid)

			break;

		case FIN:
			// se quiere desconectar el Kernel de forma normal. Vamos a apagarnos aca...
			// TODO: limpiarProcesamientosDeThreadsYTodasLasCosasAllocatedDeMemoria(void *cualquierCosa);
			break;

		default:
			break;
		}
	} while((stat = recv(*sock_ker, head, HEAD_SIZE, 0)) > 0);



	if (stat == -1){
		perror("Se perdio conexion con Kernel. error");
		return NULL;
	}

	//se desconecto Kernel de forma normal. Vamos a apagarnos aca...
	//todo: limpiarProcesamientosDeThreadsYTodasLasCosasAllocatedDeMemoria(void *cualquierCosa);

	freeAndNULL((void **) &head);
	return NULL;
}


/* dado un socket de CPU, maneja las acciones que de estos reciba
 */
void* cpu_handler(void *socket_cpu){

	tPackHeader *head = calloc(1, HEAD_SIZE);
	int stat;
	int *sock_cpu = (int*) socket_cpu;

	printf("Esperamos que lleguen cosas del socket CPU: %d\n", *sock_cpu);

	do {
		printf("proc: %d  \t msj: %d\n", head->tipo_de_proceso, head->tipo_de_mensaje);

		switch(head->tipo_de_mensaje){
		case SOLIC_BYTES:
			puts("Se recibio solicitud de bytes");

			if ((stat = manejarSolicitudBytes(*sock_cpu)) != 0)
				fprintf(stderr, "Fallo el manejo de la Solicitud de Byes. status: %d\n", stat);

			break;

		case ALMAC_BYTES:
			puts("Se recibio peticion de almacenamiento");

			if ((stat = manejarAlmacenamientoBytes(*sock_cpu)) != 0)
				fprintf(stderr, "Fallo el manejo de la Almacenamiento de Byes. status: %d\n", stat);

			break;

		case INSTR:
			puts("Se recibio pedido de instrucciones");

			if ((stat = manejarSolicitudBytes(*sock_cpu)) != 0)
				fprintf(stderr, "Fallo el manejo de la Solicitud de Byes. status: %d\n", stat);

			break;
		default:
			puts("Se recibio un mensaje no considerado");
			break;
		}
	} while((stat = recv(*sock_cpu, head, sizeof *head, 0)) > 0);

	if (stat == -1){
		perror("Fallo el recv de un mensaje desde CPU. error");
		return NULL;
	}

	printf("El CPU de socket %d cerro su conexion. Cerramos el thread\n", *sock_cpu);
	return NULL;
}
