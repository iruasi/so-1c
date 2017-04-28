#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <commons/config.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h> //inet_addr
#include <unistd.h>    //write

#include "../Compartidas/funcionesCompartidas.c"
#include "../Compartidas/tiposErrores.h"
#include "memoriaConfigurators.h"

#define MAX_IP_LEN 16 // Este largo solo es util para IPv4
#define MAX_PORT_LEN 6

#define MAXMSJ 100 // largo maximo de mensajes a enviar. Solo utilizado para 1er checkpoint
#define MAX_SIZE 100



void* connection_handler(void *);


int *setupMemoria(int quant, int size){

	// dividimos la memoria fisica en una cantidad quant de frames de tamanio size
	int *frames = malloc(quant * size);

	// setteo del Heap MetaData
	tHeapMeta *hmd = malloc(sizeof hmd->isFree + sizeof hmd->size);
	hmd->isFree = true;
	hmd->size = size - 5;

	memcpy(frames, hmd, hmd->size);

	free(hmd);
	return frames;
}


int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	char buf[MAXMSJ];
	int stat;
	int sock_kern;
	int bytes_sent;

	tMemoria *memoria = getConfigMemoria(argv[1]);
	mostrarConfiguracion(memoria);

	// creamos la "MEMORIA FISICA"
	int *MEM_FIS = setupMemoria(memoria->marcos, memoria->marco_size);


	printf("bytes disponibles en MEM_FIS: %d\n MEM_FIS esta libre: %d\n", *MEM_FIS, (uint8_t) MEM_FIS[1]);

	//sv multihilo

	int socket_desc , client_sock , c , *new_sock;
	struct sockaddr_in server , client;

	//Crea socket
	socket_desc = socket(AF_INET , SOCK_STREAM , 0);
	if (socket_desc == -1)
	{
	    printf("No pudo crear el socket");
	 }
	puts("Socket creado");

	//Prepara la structura innadr
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(5002);
	//Bind
	if( bind(socket_desc, (struct sockaddr *) &server , sizeof(server)) < 0)
	{
		//imprime error de bind
		perror("bind fallo. error");
		return 1;
	}
	puts("bind creado");

	//Listen
	listen(socket_desc , 3);

	//acepta y escucha comunicaiones
	puts("esperando comunicaciones entrantes...");
	c = sizeof(struct sockaddr_in);

	while((client_sock = accept(socket_desc, (struct sockaddr*) &client, (socklen_t*) &c)))
	{
		puts("Conexion aceptada");

		pthread_t sniffer_thread;
		new_sock = malloc(sizeof client_sock);
		*new_sock = client_sock;

		if( pthread_create(&sniffer_thread ,NULL , (void*) connection_handler ,(void*) new_sock) < 0)
		{
			perror("no pudo crear hilo");
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
    char msjAEnviar[MAXMSJ];
    clearBuffer(buf, MAXMSJ);


    /*while ((stat = recv(sock, buf, MAXMSJ, 0)) > 0)
    {

    	printf("%s\n", buf);
    	clearBuffer(buf, MAXMSJ);
    }*/

    t_Package package;
    int status;
    stat = recieve_and_deserialize(&package,sock);
    printf("AAA%d\n",stat);
    strcpy(msjAEnviar, "Hola soy Memoria\n");
    bytes_sent = send(sock,msjAEnviar, sizeof(msjAEnviar), 0);
    printf("Se enviaron: %d bytes a socket nro %d \n", bytes_sent,sock);



    if (bytes_sent == -1){
    	printf("Error en la recepcion de datos!\n valor retorno recv: %d \n", bytes_sent);
    	return FALLO_RECV;
    }

    if(stat < 0)
    	perror("recv failed");
    while(1);
    puts("Client Disconnected");
    close(sock);
    return 0;
}

int recieve_and_deserialize(t_Package *package, int socketCliente){

	int status;
	int buffer_size;
	char *buffer = malloc(buffer_size = sizeof(uint32_t));
	clearBuffer(buffer,buffer_size);

	uint32_t tipo_de_proceso;
	status = recv(socketCliente, buffer, sizeof(package->tipo_de_proceso), 0);
	memcpy(&(tipo_de_proceso), buffer, buffer_size);
	if (!status) return 0;

	uint32_t tipo_de_mensaje;
	status = recv(socketCliente, buffer, sizeof(package->tipo_de_mensaje), 0);
	memcpy(&(tipo_de_mensaje), buffer, buffer_size);
	if (!status) return 0;


	uint32_t message_long;
	status = recv(socketCliente, buffer, sizeof(package->message_long), 0);
	memcpy(&(message_long), buffer, buffer_size);
	if (!status) return 0;

	status = recv(socketCliente, package->message, message_long, 0);
	if (!status) return 0;

	printf("%d %d %s",tipo_de_proceso,tipo_de_mensaje,package->message);

	free(buffer);

	return status;
}
