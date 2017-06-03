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
#include "memoriaConfigurators.h"
#include "structsMem.h"

#define BACKLOG 20


void* connection_handler(void *);
void* kernel_handler(void *sock_ker);
void* cpu_handler(void *sock_cpu);

tMemoria *memoria;
tCacheEntrada *CACHE;
char *MEM_FIS;

int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	int stat;

	memoria = getConfigMemoria(argv[1]);
	mostrarConfiguracion(memoria);

	// inicializamos la "CACHE"
	if ((stat = setupCache(memoria->entradas_cache)) != 0)
		return ABORTO_MEMORIA;

	// inicializamos la "MEMORIA FISICA"
	if ((stat = setupMemoria(memoria->marcos, memoria->marco_size)) != 0)
		return ABORTO_MEMORIA;

	//sv multihilo
	pthread_t kern_thread;
	bool kernExists = false;
	int sock_entrada , client_sock , clientSize , new_sock;

	struct sockaddr_in client;
	clientSize = sizeof client;

	if ((sock_entrada = makeListenSock(memoria->puerto_entrada)) < 0){
		fprintf(stderr, "No se pudo crear un socket de listen. fallo: %d", sock_entrada);
		return FALLO_GRAL;
	};
	//Listen
	if ((stat = listen(sock_entrada , BACKLOG)) == -1){
		perror("No se pudo hacer listen del socket. error");
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
			printf("El tipo de proceso y mensaje son: %d y %d\n", head->tipo_de_proceso, head->tipo_de_mensaje);
			printf("Se recibio esto del socket: %d", client_sock);
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
			puts("Kernel quiere inicializar un programa.");

			recv(*sock_ker, &pid, sizeof (uint32_t), 0);
			recv(*sock_ker, &pageCount, sizeof (uint32_t), 0);
			if ((stat = inicializarPrograma(pid, pageCount)) != 0){
				puts("No se pudo inicializar el programa. Se aborta el programa.");
				abortar(pid);
			}

			puts("Recibimos el codigo fuente...");
			tPackSrcCode *pack_src;
			if ((pack_src = recvSourceCode(*sock_ker)) == NULL){
				puts("No se pudo recibir y almacenar el codigo fuente. Se aborta el programa.");
				abortar(pid);
			}

			freeAndNULL(pack_src);
			puts("Listo.");
			break;

		case ASIGN_PAG:
			puts("Kernel quiere asignar paginas!");

			recv(*sock_ker, &pid, sizeof pid, 0);
			recv(*sock_ker, &pageCount, sizeof pageCount, 0);

			// TODO: ESTARIA BUENO QUE ESTO ANDE
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
		// TODO: este ya se podria codificar un toque mas

		break;
	case ALMAC_BYTES:
		break;
	default:
		break;
	}

	return head;
}


void pagInvertida(){




	//tEntradaPagInv *tabla;

}



