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
#include "manejadoresMem.h"
#include "memoriaConfigurators.h"
#include "structsMem.h"


#define MAX_IP_LEN 16 // Este largo solo es util para IPv4
#define MAX_PORT_LEN 6

#define MAXMSJ 100 // largo maximo de mensajes a enviar. Solo utilizado para 1er checkpoint
#define MAX_SIZE 100

#define BACKLOG 20
#define SIZEOF_HMD 5
#define ULTIMO_HMD 0x02 // valor hexa de 1 byte, se distingue de entre true y false
// tal vez no sea necesario que sea hexa

void* connection_handler(void *);
void* kernel_handler(void *sock_ker);
void* cpu_handler(void *sock_cpu);

extern int sizeFrame;

extern tMemEntrada *CACHE;
extern uint8_t *MEM_FIS;

int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	tMemoria *memoria = getConfigMemoria(argv[1]);
	mostrarConfiguracion(memoria);

	sizeFrame = memoria->marco_size;

	// inicializamos la "CACHE"
	CACHE = setupCache(memoria->entradas_cache);

	// inicializamos la "MEMORIA FISICA"
	MEM_FIS = setupMemoria(memoria->marcos, memoria->marco_size);

	tHeapMeta *primerHMD = (tHeapMeta *) MEM_FIS;
	// Para ver bien como es que funciona bien este printf, esta bueno meterse con el gdb y mirar la memoria de cerca..
	printf("bytes disponibles en MEM_FIS: %d\n MEM_FIS esta libre: %d\n", primerHMD->size, primerHMD->isFree);


	//sv multihilo
	pthread_t kern_thread;
	bool kernExists = false;
	int sock_entrada , client_sock , clientSize , new_sock;
	int stat;
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

				if( pthread_create(&kern_thread ,NULL , (void*) kernel_handler ,(void*) &new_sock) < 0){
					perror("no pudo crear hilo. error");
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
	tPackHeader *head = malloc(HEAD_SIZE);
	tPackSrcCode *pack_src;

//	// con esto testeamos la primera recepcion de codigo fuente
//	pack_src = recvSourceCode(sock_ker);
//	inicializarPrograma(0, 1, pack_src->sourceLen, pack_src->sourceCode);
//
//	return NULL;

	do {
		switch(head->tipo_de_mensaje){

		case INI_PROG:

			recv(*sock_ker, &pid, sizeof (uint32_t), 0);
			recv(*sock_ker, &pageCount, sizeof (uint32_t), 0);
			//uint8_t * heap_proc = inicializarPrograma(pid, pageCount);

			break;

		case ASIGN_PAG:
			puts("Kernel quiere asignar paginas!");

			//recv(sock_ker, &pid, sizeof (uint32_t), 0);
			//recv(sock_ker, &pageCount, sizeof (uint32_t), 0);
			puts("Recibimos el codigo fuente primero...");
			pack_src = recvSourceCode(*sock_ker);
			puts("Ahora llamamos a inicializarPrograma()...");
			inicializarPrograma(0, 1, pack_src->sourceLen, pack_src->sourceCode);
			puts("Listo! Quedo el codigo fuente en MEM_FIS a partid del primer HMD");
			puts("Veamos si es cierto:");

			printf("%s\n", (char *) ((uint32_t) MEM_FIS + SIZEOF_HMD));

			//uint8_t * heap_proc = asignarPaginas(pid, pageCount);
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
	return head;
}

/*
  Esto maneja las conexiones de cada proceso que se le conecta
  */
void* connection_handler(void *socket_desc)
{
	//Get the socket descriptor
	int sock = *(int*) socket_desc;
	int stat;
	int bytes_sent;
	char buf[MAXMSJ];
	clearBuffer(buf, MAXMSJ);

	t_PackageRecepcion package;

	stat = recieve_and_deserialize(&package,sock);
	if (stat == FALLO_RECV)
		perror("Fallo receive and deserialize con FALLO RECV. error");

	strcpy(buf, "Hola soy Memoria\n");
	bytes_sent = send(sock,buf, sizeof(buf), 0);
	printf("Se enviaron: %d bytes a socket nro %d \n", bytes_sent, sock);
	clearBuffer(buf, MAXMSJ);

	if (bytes_sent == -1){
		printf("Error en la recepcion de datos!\n valor retorno recv: %d \n", bytes_sent);
		return (void *) FALLO_RECV;
	}

	while ((stat = recv(sock, buf, MAXMSJ, 0)) > 0){

		printf("%s\n", buf);
		clearBuffer(buf, MAXMSJ);
	}
	if (stat < 0){
		printf("Algo se recibio mal! stat = %d", stat);
		return FALLO_RECV;
	}

	puts("Client Disconnected");
	close(sock);
	return 0;
}

int recieve_and_deserialize(t_PackageRecepcion *package, int socketCliente){

	int status;
	int buffer_size;
	char *buffer = malloc(buffer_size = sizeof(uint32_t));
	clearBuffer(buffer,buffer_size);

	uint32_t tipo_de_proceso;
	status = recv(socketCliente, buffer, sizeof(package->tipo_de_proceso), 0);
	memcpy(&(tipo_de_proceso), buffer, buffer_size);
	if (status < 0) return FALLO_RECV;

	uint32_t tipo_de_mensaje;
	status = recv(socketCliente, buffer, sizeof(package->tipo_de_mensaje), 0);
	memcpy(&(tipo_de_mensaje), buffer, buffer_size);
	if (status < 0) return FALLO_RECV;


	uint32_t message_long;
	status = recv(socketCliente, buffer, sizeof(package->message_long), 0);
	memcpy(&(message_long), buffer, buffer_size);
	if (status < 0) return FALLO_RECV;

	status = recv(socketCliente, package->message, message_long, 0);
	if (status < 0) return FALLO_RECV;

	printf("%d %d %s",tipo_de_proceso,tipo_de_mensaje,package->message);

	free(buffer);

	return status;
}
