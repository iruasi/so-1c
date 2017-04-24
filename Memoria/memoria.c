#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include "memoria.h"
#include <commons/config.h>
#include <pthread.h>
#include<sys/socket.h>
#include<arpa/inet.h> //inet_addr
#include<unistd.h>    //write

#define MAX_IP_LEN 16 // Este largo solo es util para IPv4
#define MAX_PORT_LEN 6
#define NUM_CLIENT 10
#define MAXMSJ 100 // largo maximo de mensajes a enviar. Solo utilizado para 1er checkpoint
#define MAX_SIZE 100
#define FALLO_GRAL -21
#define FALLO_CONFIGURACION -22
#define FALLO_RECV -23
#define FALLO_CONEXION -25


void mostrarConfiguracion(tMemoria *datos_memoria);
void liberarConfiguracionMemoria(tMemoria *datos_memoria);
void *connection_handler(void *);

int establecerConexion(char *ip_destino, char *puerto_destino);
void setupHints(struct addrinfo *hints, int address_family, int socket_type, int flags);

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

	while(client_sock=accept(socket_desc,(struct sockaddr*)&client,(socklen_t*)&c))
	{
		puts("Conexion aceptada");

		pthread_t sniffer_thread;
		new_sock = malloc(1);
		*new_sock = client_sock;

		if( pthread_create( &sniffer_thread , NULL ,  connection_handler , (void*) new_sock) < 0)
		{
			perror("no pudo crear hilo");
			return 1;
	    }

		puts("Handler assignado");
	}

	if (client_sock < 0)
	{
		perror("accept fallo");
		return 1;
	}

	//fin sv multihilo








	liberarConfiguracionMemoria(memoria);
	return 0;
}

void setupHints(struct addrinfo *hint, int fam, int sockType, int flags){

	memset(hint, 0, sizeof *hint);
	hint->ai_family = fam;
	hint->ai_socktype = sockType;
	hint->ai_flags = flags;
}

/* Dados un ip y puerto de destino, se crea, conecta y retorna socket apto para comunicacion
 */
int establecerConexion(char *ip_dest, char *port_dest){

	int stat;
	int sock_dest; // file descriptor para el socket del destino a conectar
	struct addrinfo hints, *destInfo;

	setupHints(&hints, AF_UNSPEC, SOCK_STREAM, 0);

	if ((stat = getaddrinfo(ip_dest, port_dest, &hints, &destInfo)) != 0){
		fprintf("getaddrinfo: %s\n", gai_strerror(stat));
		return FALLO_GRAL;
	}

	if ((sock_dest = socket(destInfo->ai_family, destInfo->ai_socktype, destInfo->ai_protocol)) < 0)
		return FALLO_GRAL;

	connect(sock_dest, destInfo->ai_addr, destInfo->ai_addrlen);
	freeaddrinfo(destInfo);

	if (sock_dest < 0){
		printf("Error al tratar de conectar con Kernel!");
		return FALLO_CONEXION;
	}

	return sock_dest;
}


tMemoria *getConfigMemoria(char* ruta){
	printf("Ruta del archivo de configuracion: %s\n", ruta);
	tMemoria *memoria = malloc(sizeof(tMemoria));

	memoria->ip_kernel     = malloc(MAX_IP_LEN * sizeof memoria->ip_kernel );
	memoria->puerto_kernel = malloc(MAX_PORT_LEN * sizeof memoria->puerto_kernel);
	memoria->puerto_propio = malloc(MAX_PORT_LEN * sizeof memoria->puerto_propio);

	t_config *memoriaConfig = config_create(ruta);

	strcpy(memoria->ip_kernel,     config_get_string_value(memoriaConfig, "IP_KERNEL"));
	strcpy(memoria->puerto_kernel, config_get_string_value(memoriaConfig, "PUERTO_KERNEL"));
	strcpy(memoria->puerto_propio, config_get_string_value(memoriaConfig, "PUERTO_PROPIO"));


	memoria->marcos = config_get_int_value(memoriaConfig, "MARCOS");
	memoria->marco_size = config_get_int_value(memoriaConfig, "MARCO_SIZE");
	memoria->entradas_cache = config_get_int_value(memoriaConfig, "ENTRADAS_CACHE");
	memoria->cache_x_proc = config_get_int_value(memoriaConfig, "CACHE_X_PROC");
	memoria->retardo_memoria = config_get_int_value(memoriaConfig, "RETARDO_MEMORIA");

	config_destroy(memoriaConfig);
	return memoria;


}

void mostrarConfiguracion(tMemoria *memoria){
	printf("IP para el kernel: %s\n", memoria->ip_kernel);
	printf("Puerto Kernel: %s\n", memoria->puerto_kernel);
	printf("Puerto Propio: %s\n", memoria->puerto_propio);
	printf("Marcos %d\n", memoria->marcos);
	printf("Marcosize: %d\n", memoria->marco_size);
	printf("Entradas cache: %d\n", memoria->entradas_cache);
	printf("Cache x proc: %d\n", memoria->cache_x_proc);
	printf("Retardo memoria: %d\n", memoria->retardo_memoria);
}

void liberarConfiguracionMemoria(tMemoria *memoria){

	free(memoria->ip_kernel);
	free(memoria->puerto_kernel);
	free(memoria->puerto_propio);
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
void *connection_handler(void *socket_desc)
{
    //Get the socket descriptor
    int sock = *(int*)socket_desc;
    int n;
    int stat;
    int bytes_sent;
    int i;
    char buf[MAXMSJ];
    char sendBuff[100], client_message[2000];
    for(i = 0; i < MAXMSJ; ++i)
    			buf[i] = '\0';
    strcpy(buf, "Hola soy Memoria\n");
    bytes_sent = send(sock, buf, strlen(buf), 0);
    printf("Se enviaron: %d bytes a socket nro %d \n", bytes_sent,sock);


    while ((stat = recv(sock, buf, MAXMSJ, 0)) > 0)
    {

    	printf("%s\n", buf);

    	for (i = 0; i < MAXMSJ; ++i)
    		buf[i] = '\0';
    }

    if (bytes_sent == -1){
    	printf("Error en la recepcion de datos!\n valor retorno recv: %d \n", bytes_sent);
    	return FALLO_RECV;
    }



    /*while((n=recv(sock,client_message,2000,0))>0)
    {

    	send(sock,client_message,n,0);
    }*/
    close(sock);

    if(n==0)
    {
    	puts("Client Disconnected");
    }
    else
    {
    	perror("recv failed");
    }
    return 0;
}



