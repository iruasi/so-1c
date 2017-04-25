#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <commons/config.h>
#include <pthread.h>
#include<sys/socket.h>
#include<arpa/inet.h> //inet_addr
#include<unistd.h>    //write

#include "../Compartidas/funcionesCompartidas.c"
#include "../Compartidas/tiposErrores.h"
#include "memoria.h"

#define MAX_IP_LEN 16 // Este largo solo es util para IPv4
#define MAX_PORT_LEN 6
#define NUM_CLIENT 10
#define MAXMSJ 100 // largo maximo de mensajes a enviar. Solo utilizado para 1er checkpoint
#define MAX_SIZE 100


void mostrarConfiguracion(tMemoria *datos_memoria);
void liberarConfiguracionMemoria(tMemoria *datos_memoria);
void* connection_handler(void *);


int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	char buf[MAXMSJ];
	int i;
	int stat;
	int sock_kern;
	int bytes_sent;

	tMemoria *memoria = getConfigMemoria(argv[1]);
	mostrarConfiguracion(memoria);

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
	if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
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

		if( pthread_create(&sniffer_thread ,NULL , (int) connection_handler ,(void*) new_sock) < 0)
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


tMemoria *getConfigMemoria(char* ruta){
	printf("Ruta del archivo de configuracion: %s\n", ruta);
	tMemoria *memoria = malloc(sizeof(tMemoria));

	memoria->puerto_entrada = malloc(MAX_PORT_LEN);

	t_config *memoriaConfig = config_create(ruta);

	strcpy(memoria->puerto_entrada, config_get_string_value(memoriaConfig, "PUERTO_ENTRADA"));

	memoria->marcos =          config_get_int_value(memoriaConfig, "MARCOS");
	memoria->marco_size =      config_get_int_value(memoriaConfig, "MARCO_SIZE");
	memoria->entradas_cache =  config_get_int_value(memoriaConfig, "ENTRADAS_CACHE");
	memoria->cache_x_proc =    config_get_int_value(memoriaConfig, "CACHE_X_PROC");
	memoria->retardo_memoria = config_get_int_value(memoriaConfig, "RETARDO_MEMORIA");

	config_destroy(memoriaConfig);
	return memoria;
}

void mostrarConfiguracion(tMemoria *memoria){

	printf("Puerto Entrada: %s\n",  memoria->puerto_entrada);
	printf("Marcos %d\n",           memoria->marcos);
	printf("Marcosize: %d\n",       memoria->marco_size);
	printf("Entradas cache: %d\n",  memoria->entradas_cache);
	printf("Cache x proc: %d\n",    memoria->cache_x_proc);
	printf("Retardo memoria: %d\n", memoria->retardo_memoria);
}

void liberarConfiguracionMemoria(tMemoria *memoria){

	free(memoria->puerto_entrada);
	free(memoria->marcos);
	free(memoria->marco_size);
	free(memoria->entradas_cache);
	free(memoria->cache_x_proc);
	free(memoria->retardo_memoria);
	free(memoria);
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
    int i;
    char buf[MAXMSJ];
    clearBuffer(buf, MAXMSJ);

    strcpy(buf, "Hola soy Memoria\n");
    bytes_sent = send(sock, buf, strlen(buf), 0);
    printf("Se enviaron: %d bytes a socket nro %d \n", bytes_sent,sock);


    while ((stat = recv(sock, buf, MAXMSJ, 0)) > 0)
    {

    	printf("%s\n", buf);
    	clearBuffer(buf, MAXMSJ);
    }

    if (bytes_sent == -1){
    	printf("Error en la recepcion de datos!\n valor retorno recv: %d \n", bytes_sent);
    	return FALLO_RECV;
    }

    if(stat < 0)
    	perror("recv failed");

    puts("Client Disconnected");
    close(sock);
    return 0;
}



