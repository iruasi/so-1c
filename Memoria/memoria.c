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

#include "../Compartidas/tiposPaquetes.h"
#include "../Compartidas/funcionesCompartidas.c"
#include "../Compartidas/funcionesPaquetes.c"
#include "../Compartidas/tiposErrores.h"
#include "apiMemoria.h"
#include "manejadoresMem.h"
#include "memoriaConfigurators.h"
#include "structsMem.h"

#define BACKLOG 20


void* connection_handler(void *);
void* kernel_handler(void *sock_ker);
void* cpu_handler(void *sock_cpu);


int size_frame;
int quant_frames;

extern tCacheEntrada *CACHE;
extern uint8_t *MEM_FIS;

char *mem_ptr;

int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	int stat;

	tMemoria *memoria = getConfigMemoria(argv[1]);
	mostrarConfiguracion(memoria);

	size_frame   = memoria->marco_size;
	quant_frames = memoria->marcos;

	// inicializamos la "CACHE"
	CACHE = setupCache(memoria->entradas_cache);

	// inicializamos la "MEMORIA FISICA"
	char (*mem)[memoria->marcos][memoria->marco_size];
//	if ((stat = setupMemoria(memoria->marcos, memoria->marco_size, &mem)) != 0)
//		return ABORTO_MEMORIA;
	if ((stat = setupMemoria(3, 4, &mem)) != 0)
		return ABORTO_MEMORIA;

	mem_ptr = mem;

	escribirBytes(0, 0, 0, 0, 0);

	tHeapMeta *primerHMD = (tHeapMeta *) MEM_FIS;
	// Para ver bien como es que funciona bien este printf, esta bueno meterse con el gdb y mirar la memoria de cerca..
	printf("bytes disponibles en MEM_FIS: %d\n MEM_FIS esta libre: %d\n", primerHMD->size, primerHMD->isFree);


	//sv multihilo
	pthread_t kern_thread;
	bool kernExists = false;
	int sock_entrada , client_sock , clientSize , new_sock;

	struct sockaddr_in client;
	clientSize = sizeof client;

	sock_entrada = makeListenSock(memoria->puerto_entrada);
	//Listen
	listen(sock_entrada , BACKLOG);

	//acepta y escucha comunicaciones
	puts("esperando comunicaciones entrantes...");

	while((client_sock = accept(sock_entrada, (struct sockaddr*) &client, (socklen_t*) &clientSize)))
	{
		puts("Conexion aceptada");

		pthread_t sniffer_thread;
		new_sock = client_sock; // los copiamos por valor

		tPackHeader *head = malloc(HEAD_SIZE);
		if ((stat = recv(client_sock, head, HEAD_SIZE, 0)) < 0){
			perror("Error en la recepcion de handshake. error");
			return FALLO_RECV;
		}

		switch(head->tipo_de_proceso){
		case KER:

			if (!kernExists){
				kernExists = true;

				if ((stat = contestarMemoriaKernel(memoria->marco_size, memoria->marcos, new_sock)) == -1){
					puts("No se pudo enviar la informacion relevante a Kernel!");
					return FALLO_GRAL;
				}

				if( pthread_create(&kern_thread, NULL, (void*) kernel_handler, (void*) &new_sock) < 0){
					perror("No pudo crear hilo. error");
					return FALLO_GRAL;
				}

			} else {
				errno = CONEX_INVAL;
				perror("Se trato de conectar otro Kernel. error");
			}

			break;

		case CPU:
			if( pthread_create(&sniffer_thread ,NULL , (void*) cpu_handler ,(void*) &new_sock) < 0){
				perror("no pudo crear hilo. error");
				return FALLO_GRAL;
			}
			break;

		default:
			puts("Trato de conectarse algo que no era ni Kernel ni CPU!");
			return CONEX_INVAL;
		}
		continue;

		connection_handler((void*) &new_sock);
		if( pthread_create(&sniffer_thread ,NULL , (void*) connection_handler ,(void*) &new_sock) < 0)
		{
			perror("no pudo crear hilo. error");
			return FALLO_GRAL;
		}

		puts("Handler assignado");
	}

	if (client_sock < 0)
	{
		perror("accept fallo\n");
		puts("Deberia considerarse una posible INTR al momento de accept()..\n");
		return FALLO_GRAL;
	}

	//fin sv multihilo


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
	tPackSrcCode *pack_src;

	printf("Esperamos que lleguen cosas del socket: %d\n", *sock_ker);

	do {
		printf("Se hace una pasada\n head tiene: %d y %d\n", head->tipo_de_proceso, head->tipo_de_mensaje);
		switch(head->tipo_de_mensaje){

		case INI_PROG:

			recv(*sock_ker, &pid, sizeof (uint32_t), 0);
			recv(*sock_ker, &pageCount, sizeof (uint32_t), 0);
			void *segs_location = inicializarPrograma(pid, pageCount);

			break;

		case ALMAC_BYTES:
			puts("Kernel quiere escribir codigo fuente");



			break;

		case ASIGN_PAG:
			puts("Kernel quiere asignar paginas!");

			//recv(sock_ker, &pid, sizeof (uint32_t), 0);
			//recv(sock_ker, &pageCount, sizeof (uint32_t), 0);
			puts("Recibimos el codigo fuente...");
			pack_src = recvSourceCode(*sock_ker);

			puts("Ahora llamamos a inicializarProgramaBeta()...");
			inicializarProgramaBeta(0, 1, pack_src->sourceLen, pack_src->sourceCode);
			puts("Listo!");

			dump(NULL);

			uint8_t * heap_proc = asignarPaginas(pid, pageCount);
			break;
		case FIN_PROG:
			break;

		case FIN:
			//se quiere desconectar el Kernel de forma normal. Vamos a apagarnos aca...
			//todo: limpiarProcesamientosDeThreadsYTodasLasCosasAllocatedDeMemoria(void *cualquierCosa);
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

	return head;
}

/* dado un socket de CPU, maneja las acciones que de estos reciba
 */
void* cpu_handler(void *sock_ker){

	tPackHeader *head = malloc(HEAD_SIZE);

	int x = (int) sock_ker;
	x++;

	while(1){
		printf("%d\n", x);
		sleep(10);
	}

	switch(head->tipo_de_mensaje){
	case SOLIC_BYTES:
		break;
	case ALMAC_BYTES:
		break;
	default:
		break;
	}

	return head;
}


void pagInvertida(){ // TODO: estoy medio perdido con esto...




	//tEntradaPagInv *tabla;

}



