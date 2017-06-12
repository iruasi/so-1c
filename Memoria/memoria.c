#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <netdb.h>
#include <commons/config.h>
#include <commons/log.h>
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

t_log * logger;

void crearLoggerMemoria(){
	 char * archivoLog = strdup("MEMORIA_LOG.cfg");
	 logger = log_create("MEMORIA_LOG.cfg",archivoLog,true,LOG_LEVEL_INFO);
	 free(archivoLog);archivoLog = NULL;
}

#define BACKLOG 20

void* kernel_handler(void *sock_ker);
void* cpu_handler(void *sock_cpu);

tMemoria *memoria;          // configuracion del proceso Memoria
char *MEM_FIS;              // Memoria Fisica
char *CACHE;                // memoria CACHE
tCacheEntrada *CACHE_lines; // vector de lineas a CACHE
int  *CACHE_accs;           // vector de accesos a CACHE

int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	int stat;

	crearLoggerMemoria();

	memoria = getConfigMemoria(argv[1]);
	mostrarConfiguracion(memoria);

	// inicializamos la "CACHE"
	if ((stat = setupCache()) != 0)
		return ABORTO_MEMORIA;

	// inicializamos la "MEMORIA FISICA"
	if ((stat = setupMemoria()) != 0)
		return ABORTO_MEMORIA;

	//sv multihilo
	pthread_t kern_thread;
	bool kernExists = false;
	int sock_entrada , client_sock , clientSize , new_sock;

	struct sockaddr_in client;
	clientSize = sizeof client;

	if ((sock_entrada = makeListenSock(memoria->puerto_entrada)) < 0){

		log_error(logger,"No se pudo crear un socket de listen. Fallo: %d",sock_entrada);
		return FALLO_GRAL;
	};
	//Listen
	if ((stat = listen(sock_entrada , BACKLOG)) == -1){

		log_error(logger,"No se pudo hacer listen del socket");
		return FALLO_GRAL;
	};

	//acepta y escucha comunicaciones
	puts("esperando comunicaciones entrantes...");
	while((client_sock = accept(sock_entrada, (struct sockaddr*) &client, (socklen_t*) &clientSize)) != -1)
	{
		puts("Conexion aceptada");

		pthread_t sniffer_thread;
		new_sock = client_sock; // los copiamos por valor

		tPackHeader *head = malloc(HEAD_SIZE);
		if ((stat = recv(client_sock, head, HEAD_SIZE, 0)) < 0){
			log_error(logger,"Error en la recepcion de handshake");

			return FALLO_RECV;
		}

		switch(head->tipo_de_proceso){
		case KER:

			if (!kernExists){
				kernExists = true;

				if ((stat = contestarMemoriaKernel(memoria->marco_size, memoria->marcos, new_sock)) == -1){

					log_error(logger,"No se pudo enviar informacion relevante al Kernel");
					return FALLO_GRAL;
				}

				if( pthread_create(&kern_thread, NULL, (void*) kernel_handler, (void*) &new_sock) < 0){

					log_error(logger,"No se pudo crear hilo");
					return FALLO_GRAL;
				}

			} else {
				errno = CONEX_INVAL;
				log_error(logger,"Se trato de conectar otro kernel");

			}

			break;

		case CPU:
			if( pthread_create(&sniffer_thread ,NULL , (void*) cpu_handler ,(void*) &new_sock) < 0){
				log_error(logger,"No se pudo crear el hilo");

				return FALLO_GRAL;
			}
			break;

		default:
			log_info(logger,"Trato de conectarse algo que no era ni kernel ni CPU");
			log_info(logger,"El tipo de proceso y mensaje son: %d y %d",head->tipo_de_proceso,head->tipo_de_mensaje);
			log_info(logger,"Se recibio esto del socket: %d", client_sock);

			return CONEX_INVAL;
		}
	}

	// Si salio del ciclo es porque fallo el accept()
	perror("Fallo el accept(). error");

	liberarConfiguracionMemoria(memoria);
	return 0;
}


/* dado el socket de Kernel, maneja las acciones que de este reciba
 */
void* kernel_handler(void *sock_kernel){

	int *sock_ker = (int *) sock_kernel;
	int stat;
	int pid, pageCount;

	tPackHeader *head = calloc(HEAD_SIZE, 1);

	printf("Esperamos que lleguen cosas del socket: %d\n", *sock_ker);

	do {
		switch(head->tipo_de_mensaje){
		case INI_PROG:

			log_info(logger,"El kernel quiere incializar un programa");

			recv(*sock_ker, &pid, sizeof (uint32_t), 0);
			recv(*sock_ker, &pageCount, sizeof (uint32_t), 0);
			if ((stat = inicializarPrograma(pid, pageCount)) != 0){


				log_error(logger,"No se pudo inicializar el programa. Se aborta el programa");
				abortar(pid);
			}
		log_info(logger,"Recibimos el codigo fuente");
			tPackSrcCode *pack_src;
			if ((pack_src = recvSourceCode(*sock_ker)) == NULL){


				log_error(logger,"No se pudo recibir y almacenar el codigo fuente. Se aborta el programa");
				abortar(pid);
			}

			freeAndNULL((void **) &pack_src);

			log_info(logger,"Listo");
			break;

		case ASIGN_PAG:

			log_info(logger,"Kernel quiere asignar paginas");
			recv(*sock_ker, &pid, sizeof pid, 0);
			recv(*sock_ker, &pageCount, sizeof pageCount, 0);

			asignarPaginas(pid, pageCount);

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
		log_error(logger,"Se perdio conexion con el Kernel");

		return NULL;
	}

	//se desconecto Kernel de forma normal. Vamos a apagarnos aca...
	//todo: limpiarProcesamientosDeThreadsYTodasLasCosasAllocatedDeMemoria(void *cualquierCosa);

	return head;
}

/* dado un socket de CPU, maneja las acciones que de estos reciba
 */
void* cpu_handler(void *socket_cpu){

	tPackHeader *head = malloc(HEAD_SIZE);
	int stat;
	int *sock_cpu = (int*) socket_cpu;


	log_info(logger,"Esperamos que lleguen cosas del socket: %d", sock_cpu);

	do {
		switch(head->tipo_de_mensaje){
		case SOLIC_BYTES:
			log_info(logger,"Se recibio solicitud de bytes");

			// TODO: este ya se podria codificar un toque mas
			break;

		case ALMAC_BYTES:
			log_info(logger,"Se recibio peticion de almacenamiento");

			break;

		default:
			log_info(logger,"Se recibio un mensaje no considerado");

			break;
		}
	} while((stat = recv(*sock_cpu, head, sizeof *head, 0)) > 0);

	if (stat == -1){
		log_error(logger,"Fallo el recv de un mensaje desde CPU");

		return NULL;
	}
	log_info(logger,"El CPU de socket %d cerro su conexion. Se cierra el thread",sock_cpu);

	return NULL;
}
