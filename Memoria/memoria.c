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
#include <semaphore.h>

#include <tiposRecursos/tiposErrores.h>
#include <tiposRecursos/tiposPaquetes.h>
#include <funcionesPaquetes/funcionesPaquetes.h>
#include <funcionesCompartidas/funcionesCompartidas.h>

#include <commons/log.h>

#include "apiMemoria.h"
#include "auxiliaresMemoria.h"
#include "manejadoresMem.h"
#include "manejadoresCache.h"
#include "memoriaConfigurators.h"
#include "structsMem.h"
#include "flujosMemoria.h"

#define BACKLOG 20
#define MAXOPCION 200

void* kernel_handler(void *sock_ker);
void* cpu_handler(void *sock_cpu);

tMemoria *memoria;          // configuracion del proceso Memoria
char *MEM_FIS;              // Memoria Fisica
char *CACHE;                // memoria CACHE
tCacheEntrada *CACHE_lines; // vector de lineas a CACHE
int  *CACHE_accs;           // vector de accesos hechos a CACHE

int sock_kernel;
t_log * logTrace;
pthread_mutex_t mux_mem_access;

struct infoKer{
	int *sock_ker;
	bool kernExists;
};


int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	logTrace = log_create("/home/utnso/logMEMORIATrace.txt","MEMORIA",0,LOG_LEVEL_TRACE);

	log_trace(logTrace,"");
	log_trace(logTrace,"");
	log_trace(logTrace,"");
	log_trace(logTrace,"");
	log_trace(logTrace,"");
	log_trace(logTrace,"");
	log_trace(logTrace,"");
	log_trace(logTrace,"Inicia nueva ejecucion de MEMORIA");
	log_trace(logTrace,"");
	log_trace(logTrace,"");
	log_trace(logTrace,"");
	log_trace(logTrace,"");
	log_trace(logTrace,"");
	log_trace(logTrace,"");


	int stat;

	pthread_mutex_init(&mux_mem_access, NULL);
	memoria = getConfigMemoria(argv[1]);
	mostrarConfiguracion(memoria);

	if ((stat = setupMemoria()) != 0)
		return ABORTO_MEMORIA;

	pthread_t kern_thread;
	pthread_t consMemoria_thread;
	struct infoKer infoKer;
	infoKer.kernExists = false;
	int sock_entrada , client_sock , clientSize;

	struct sockaddr_in client;
	clientSize = sizeof client;


	if( pthread_create(&consMemoria_thread, NULL, (void*) consolaMemoria,NULL) < 0){
		log_error(logTrace,"no se pudo crear el hilo");
		perror("No pudo crear hilo. error");
		return FALLO_GRAL;
	}

	if ((sock_entrada = makeListenSock(memoria->puerto_entrada)) < 0){
		log_error(logTrace,"no se pudo crear un socket de listen");
		fprintf(stderr, "No se pudo crear un socket de listen. fallo: %d", sock_entrada);
		return FALLO_GRAL;
	}
	//Listen
	if ((stat = listen(sock_entrada , BACKLOG)) == -1){
		log_error(logTrace,"no se pudo hacer listen del socket");
		perror("No se pudo hacer listen del socket. error");
		return FALLO_GRAL;
	}

	//acepta y escucha comunicaciones
	tPackHeader head;
	//puts("esperando comunicaciones entrantes...");
	log_trace(logTrace,"esperando comunicaciones entrantes");
	while((client_sock = accept(sock_entrada, (struct sockaddr*) &client, (socklen_t*) &clientSize)) != -1){
		//puts("Conexion aceptada");
		log_trace(logTrace,"conexion aceptada");
		if ((stat = recv(client_sock, &head, HEAD_SIZE, 0)) < 0){
			log_error(logTrace,"error en la recepcion de HS");
			perror("Error en la recepcion de handshake. error");
			return FALLO_RECV;
		}

		switch(head.tipo_de_proceso){

		case KER:

			if (!infoKer.kernExists){
				infoKer.sock_ker   = malloc(sizeof(int));
				*infoKer.sock_ker  = client_sock;
				infoKer.kernExists = true;

				head.tipo_de_proceso = MEM; head.tipo_de_mensaje = MEMINFO;
				if ((stat = contestar2ProcAProc(head, memoria->marcos, memoria->marco_size, *infoKer.sock_ker)) == -1){
					log_error(logTrace,"no se pudo enviar la info relevante a kernel");
					puts("No se pudo enviar la informacion relevante a Kernel!");
					return FALLO_GRAL;
				}

				if(pthread_create(&kern_thread, NULL, (void*) kernel_handler, (void*) &infoKer) < 0){
					log_error(logTrace,"no se pudo crear hilo");
					perror("No pudo crear hilo. error");
					return FALLO_GRAL;
				}

			} else
				puts("Se trato de conectar otro Kernel. Ignoramos el paquete...");
			log_trace(logTrace,"se trato de conextar otro kernel. lo igrnoramos");
			break;

		case CPU:
			puts("Ingresa CPU");
			log_trace(logTrace,"ingresa cpu");
			pthread_t cpu_thread;
			int *sock_cpu = malloc(sizeof(int));
			*sock_cpu    = client_sock;

			head.tipo_de_proceso = MEM; head.tipo_de_mensaje = MEMINFO;
			if ((stat = contestarProcAProc(head, memoria->marco_size, *sock_cpu)) == -1){
				log_error(logTrace,"no se pudo enviar la info relevante al kernel");
				puts("No se pudo enviar la informacion relevante a Kernel!");
				return FALLO_GRAL;
			}

			if( pthread_create(&cpu_thread, NULL, (void*) cpu_handler, (void*) sock_cpu) < 0){
				log_error(logTrace,"no pudo crear el hilo");
				perror("no pudo crear hilo. error");
				return FALLO_GRAL;
			}
			break;

		default:
			puts("Trato de conectarse algo que no era ni Kernel ni CPU!");
			//printf("El tipo de proceso y mensaje son: %d y %d\n", head.tipo_de_proceso, head.tipo_de_mensaje);
			//printf("Se recibio esto del socket: %d\n", client_sock);
			log_trace(logTrace,"trato de conectarse algo q no era kernel ni cpu");
			return CONEX_INVAL;
		}
	}

	// Si salio del ciclo es porque fallo el accept()
	log_error(logTrace,"fallo el accept");
	perror("Fallo el accept(). error");

	liberarConfiguracionMemoria();
	return 0;
}


/* dado el socket de Kernel, maneja las acciones que de este reciba
 */
void* kernel_handler(void *infoKer){

	struct infoKer *ik = (struct infoKer *) infoKer;
	sock_kernel = *ik->sock_ker;
	int stat, new_page, pack_size;
	char *buffer;

	tPackHeader head = {.tipo_de_proceso = KER, .tipo_de_mensaje = THREAD_INIT};
	tPackPidPag *pp;
	tPackPID *ppid;

	//printf("(KERNEL_THR) Esperamos cosas del socket Kernel: %d\n", *ik->sock_ker);
	log_trace(logTrace,"kernel thr");
	do {

		switch(head.tipo_de_mensaje){
		case INI_PROG:
			//puts("Kernel quiere inicializar un programa.");
			log_trace(logTrace,"kernel manda ini_prog");
			if ((buffer = recvGeneric(*ik->sock_ker)) == NULL)
				break;

			if ((pp = deserializePIDPaginas(buffer)) == NULL)
				break;
			freeAndNULL((void **) &buffer);

			pthread_mutex_lock(&mux_mem_access);
			stat = inicializarPrograma(pp->pid, pp->pageCount);
			pthread_mutex_unlock(&mux_mem_access);

			freeAndNULL((void **) &pp);
			if (stat != 0){
				log_info(logTrace,"no se pudo inicializar el programa");
				puts("No se pudo inicializar el programa");
			}
			//puts("Fin case INI_PROG.");
			break;

		case BYTES:
			//puts("Kernel quiere Solicitar Bytes");
			log_trace(logTrace,"kernel solicita bytes");
			pthread_mutex_lock(&mux_mem_access);
			stat = manejarSolicitudBytes(*ik->sock_ker);
			pthread_mutex_unlock(&mux_mem_access);

			if (stat != 0){
				log_error(logTrace,"fallo el manejod e la solicitud de bytes");
				printf("Fallo el manejo de la Solicitud de Bytes. status: %d\n", stat);
				head.tipo_de_proceso = MEM; head.tipo_de_mensaje = stat;
				informarResultado(*ik->sock_ker, head);

			} else
				//puts("Se completo Solicitud de Bytes");
				log_trace(logTrace,"solictud de bytes completada");
			break;

		case ALMAC_BYTES:
			//puts("Kernel quiere almacenar bytes");
			log_trace(logTrace,"kernel quiere almacenar bytes");
			pthread_mutex_lock(&mux_mem_access);
			stat = manejarAlmacenamientoBytes(*ik->sock_ker);
			pthread_mutex_unlock(&mux_mem_access);

			if (stat != 0){
				log_error(logTrace,"fallo el manejo de almacenamieto de bytes");
				printf("Fallo el manejo de la Almacenamiento de Bytes. status: %d\n", stat);
			}
			//puts("Fin case ALMAC_BYTES.");
			break;

		case ASIGN_PAG:
			//puts("Kernel quiere asignar paginas!");
			log_trace(logTrace,"kernel quier asignar pags");
			if ((buffer = recvGeneric(*ik->sock_ker)) == NULL)
				break;

			if ((pp = deserializePIDPaginas(buffer)) == NULL)
				break;
			freeAndNULL((void **) &buffer);

			pthread_mutex_lock(&mux_mem_access);
			new_page = asignarPaginas(pp->pid, pp->pageCount);
			pthread_mutex_unlock(&mux_mem_access);

			pp->head.tipo_de_mensaje = (new_page < 0)? FALLO_ASIGN : ASIGN_SUCCS;
			pp->head.tipo_de_proceso = MEM;
			pp->pageCount = new_page;
			pack_size = 0;
			buffer = serializePIDPaginas(pp, &pack_size);

			if ((stat = send(*ik->sock_ker, buffer, pack_size, 0)) == -1){
				log_error(logTrace,"fallo send de pag asignada a kernel");
				perror("Fallo send de pagina asignada a Kernel. error");
				break;
			}
			//printf("Se enviaron %d bytes del paquete PIDPaginas a Kernel\n", stat);
			log_trace(logTrace,"se enviaron %d bytes de pidpaginas a kernel",stat);
			freeAndNULL((void **) &pp);
			freeAndNULL((void **) &buffer);
			//puts("Fin case ASIGN_PAG.");
			break;

		case FIN_PROG:
			log_trace(logTrace,"case fin_prog");
			if ((buffer = recvGeneric(*ik->sock_ker)) == NULL)
				break;

			if ((ppid = deserializeVal(buffer)) == NULL)
				break;

			finalizarPrograma(ppid->val);
			break;

		default:
			break;
		}
	} while((stat = recv(*ik->sock_ker, &head, HEAD_SIZE, 0)) > 0);

	if (stat == -1){
		log_error(logTrace,"se perdio conexion con kernel.");
		perror("Se perdio conexion con Kernel. error");
		return NULL;
	}

	log_trace(logTrace,"kernel cerro la conexion");
	puts("Kernel cerro la conexion.");
	ik->kernExists = false;
	close(*ik->sock_ker);
	freeAndNULL((void **) &ik->sock_ker);
	return NULL;
}


/* dado un socket de CPU, maneja las acciones que de estos reciba
 */
void* cpu_handler(void *socket_cpu){

	tPackHeader head = {.tipo_de_proceso = CPU, .tipo_de_mensaje = THREAD_INIT};
	int stat;
	int *sock_cpu = (int*) socket_cpu;

	//printf("Esperamos que lleguen cosas del socket CPU: %d\n", *sock_cpu);
	log_trace(logTrace,"thrad cpu handler. a la esprea de msjs");
	do {
		//printf("proc: %d  \t msj: %d\n", head.tipo_de_proceso, head.tipo_de_mensaje);

		switch(head.tipo_de_mensaje){
		case BYTES:
			log_trace(logTrace,"cpu solicita bytes");
			//puts("CPU quiere Solicitar Bytes");
			pthread_mutex_lock(&mux_mem_access);

			if ((stat = manejarSolicitudBytes(*sock_cpu)) != 0){
				log_info(logTrace,"fallo el manejo de la solicitud debytes ");
				fprintf(stderr, "Fallo el manejo de la Solicitud de Bytes. status: %d\n", stat);
			}
			pthread_mutex_unlock(&mux_mem_access);
			log_trace(logTrace,"fin solciitud d ebytes");
			//puts("Se completo Solicitud de Bytes");
			break;

		case ALMAC_BYTES:
			//puts("CPU quiere Almacenar bytes CPU");
			log_trace(logTrace,"cpu quiere almacenar bytes");
			pthread_mutex_lock(&mux_mem_access);

			if ((stat = manejarAlmacenamientoBytes(*sock_cpu)) != 0){
				log_error(logTrace,"fallo el manejo de almacenamiento de bytes");
				fprintf(stderr, "Fallo el manejo de la Almacenamiento de Byes. status: %d\n", stat);
			}
			pthread_mutex_unlock(&mux_mem_access);
			//puts("Se completo Peticion de Almacenamiento CPU");
			log_trace(logTrace,"peticion de bytes completada");
			break;

		case INSTR:
			//puts("Se recibio pedido de instrucciones");
			log_trace(logTrace,"se recibio pedido de instrucciones");
			pthread_mutex_lock(&mux_mem_access);

			if ((stat = manejarSolicitudBytes(*sock_cpu)) != 0){
				log_error(logTrace,"fallo el manejo de la solicitud de bytes");
				fprintf(stderr, "Fallo el manejo de la Solicitud de Bytes. status: %d\n", stat);
			}
			pthread_mutex_unlock(&mux_mem_access);
			break;

		default:
			//puts("Se recibio un mensaje no considerado");
			log_trace(logTrace,"se recibio un msk no considerados");
			break;
		}
	} while((stat = recv(*sock_cpu, &head, HEAD_SIZE, 0)) > 0);

	if (stat == -1){
		log_error(logTrace,"fallo el recv de un mensaje desde cpu");
		perror("Fallo el recv de un mensaje desde CPU. error");
		return NULL;
	}

	printf("El CPU de socket %d cerro su conexion. Cerramos el thread\n", *sock_cpu);
	log_trace(logTrace,"cpu cierra su conexion");
	return NULL;
}

void consolaMemoria(void){

	printf("\n \n \nIngrese accion a realizar:\n");
	printf ("1-Para Modificar retardo: 'retardo <ms>'\n");
	printf ("2-Para Generar reporte del estado actual: 'dump'\n");
	printf ("3-Para limpiar la cache: 'flush'\n");
	printf ("4-Para ver size de la memoria: 'sizeMemoria'\n");
	printf ("5-Para ver size de un proceso: 'sizeProceso <PID>'\n");


	int finalizar = 0;
	while(finalizar != 1){
		printf("Seleccione opcion: \n");
		char opcion[MAXOPCION];
		fgets(opcion, MAXOPCION, stdin);
		opcion[strlen(opcion) - 1] = '\0';

		if (strncmp(opcion, "retardo", 7) == 0){
			puts("Opcion retardo");
			log_trace(logTrace,"opcion retardo");
			char *msChar = opcion + 8;
			int ms = atoi(msChar);
			retardo(ms);

		} else if (strncmp(opcion, "dump", 4) == 0){
			puts("Opcion dump");
			log_trace(logTrace,"opcion dump");
			char *dpChar = opcion + 5;
			int dp = atoi(dpChar);
			dump(dp);

		} else if (strncmp(opcion, "flush", 5) == 0){
			puts("Opcion flush");
			log_trace(logTrace,"opcion flush");
			flush();

		} else if (strncmp(opcion, "sizeMemoria", 11) == 0){
			puts("Opcion sizeMemoria");
			log_trace(logTrace,"opcion size memoria");
			size(-1);

		} else if (strncmp(opcion, "sizeProceso", 11) == 0){
			puts("Opcion sizeProceso");
			log_trace(logTrace,"opcion sizeproceso");
			char *pidProceso = opcion+12;
			int pid = atoi(pidProceso);
			size(pid);
		}
	}
}

