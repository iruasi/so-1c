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

#define SIZEOF_HMD 5
#define ULTIMO_HMD 0x02 // valor hexa de 1 byte, se distingue de entre true y false
// tal vez no sea necesario que sea hexa

void* connection_handler(void *);

uint8_t *setupMemoria(int quantity, int size);

uint8_t *reservarBytes(int sizeReserva);
uint8_t esReservable(int size, tHeapMeta *heapMetaData);
tHeapMeta *crearNuevoHMD(uint8_t *dir_mem, int size);

uint8_t *obtenerFrame(uint32_t pid, uint32_t page);

uint32_t almacenarBytes(uint32_t pid, uint32_t page, uint32_t offset, uint32_t size, uint8_t* buffer);
void escribirBytes(int pid, int frame, int offset, int size, void* buffer); // todo: escribir implementacion

uint8_t *solicitarBytes(uint32_t pid, uint32_t page, uint32_t offset, uint32_t size);
uint8_t *leerBytes(uint32_t pid, uint32_t frame, uint32_t offset, uint32_t size);

int sizeFrame, sizeMaxResrv;

uint8_t *CACHE;
uint8_t *MEM_FIS;

int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	tMemoria *memoria = getConfigMemoria(argv[1]);
	mostrarConfiguracion(memoria);

	sizeFrame = memoria->marco_size;
	sizeMaxResrv = sizeFrame - 2 * SIZEOF_HMD;

	// inicializamos la "CACHE"
	CACHE = setupMemoria(memoria->marcos, memoria->marco_size);
	// inicializamos la "MEMORIA FISICA"
	MEM_FIS = setupMemoria(memoria->marcos, memoria->marco_size);

	// Para ver bien como es que funciona bien este printf, esta bueno meterse con el gdb y mirar la memoria de cerca..
	printf("bytes disponibles en MEM_FIS: %d\n MEM_FIS esta libre: %d\n",  * (uint16_t*) MEM_FIS, MEM_FIS[4]);

	uint8_t *loc = MEM_FIS;
	loc = reservarBytes(24);
	printf("bytes disponibles en loc: %d\n loc esta libre: %d\n",  * (uint16_t*) loc, loc[4]);

	uint8_t *loc2 = MEM_FIS;
	loc2 = reservarBytes(655);
	printf("bytes disponibles en loc2: %d\n loc2 esta libre: %d\n",  * (uint16_t*) loc2, loc2[4]);

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


/* Retorna un puntero a un espacio de MEM_FIS donde se podran escribir bytes
 * Retorna NULL si no fue posible reservar espacio
 */
uint8_t *reservarBytes(int sizeReserva){
// por ahora trabaja con la unica pagina que existe

	tHeapMeta *hmd = (tHeapMeta *) MEM_FIS;
	int sizeLibre = sizeMaxResrv;

	uint8_t rta = esReservable(sizeReserva, hmd);
	while(rta != ULTIMO_HMD){

		if (rta == true){

			hmd->size = sizeReserva;
			hmd->isFree = false;

			sizeLibre -= sizeReserva;
			uint8_t* dirNew_hmd = (uint8_t *) ((uint32_t) hmd + SIZEOF_HMD + hmd->size);
			crearNuevoHMD(dirNew_hmd, sizeLibre);

			return (uint8_t *) hmd;
		}

		sizeLibre -= hmd->size;

		uint8_t* dir = (uint8_t *) ((uint32_t) hmd + SIZEOF_HMD + hmd->size);
		hmd = (tHeapMeta *) dir;
		rta = esReservable(sizeReserva, hmd);
	}

	return NULL; // en esta unica pagina no tuvimos espacio para reservar sizeReserva
}

/* Esta funcion deberia usarse solamente para crear el HMD al final de una reserva en heap.
 * Crea un nuevo HMD de tamanio size en una direccion de memoria, y por defecto esta libre
 */
tHeapMeta *crearNuevoHMD(uint8_t *dir_mem, int size){

	tHeapMeta *new_hmd = (tHeapMeta *) dir_mem;
	new_hmd->size = size;
	new_hmd->isFree = true;

	return new_hmd;
}

/* Nos dice, dado un HMD, si su espacio de memoria es reservable por una cantidad size de bytes
 */
uint8_t esReservable(int size, tHeapMeta *hmd){

	if(! hmd->isFree || hmd->size < size)
		return false;

	else if (hmd->size == 0) // esta libre y con espacio cero => es el ultimo MetaData
		return ULTIMO_HMD;

	return true;
}

/* Crea una cantidad quant de frames de tamanio size.
 * Se usa para la configuracion del espacio de memoria inicial
 */
uint8_t *setupMemoria(int quant, int size){
	// dividimos la memoria fisica en una cantidad quant de frames de tamanio size

	uint8_t *frames = malloc(quant * size);

	// setteo del Heap MetaData
	tHeapMeta *hmd = malloc(sizeof hmd->isFree + sizeof hmd->size);
	hmd->isFree = true;
	hmd->size = size - SIZEOF_HMD;

	memcpy(frames, (uint8_t*) hmd, hmd->size);

	free(hmd);
	return frames;
}




uint32_t almacenarBytes(uint32_t pid, uint32_t page, uint32_t offset, uint32_t size, uint8_t* buffer){

	uint8_t *frame = obtenerFrame(pid, page);
	escribirBytes(pid, frame, offset, size, buffer);
}


/* Esta es la funcion que en el TP viene a ser "Almacenar Bytes en una Pagina"
 */
void escribirBytes(int pid, int frame, int offset, int size, void* buffer){
// De momento tenemos una unica pagina, por lo que solo nos importa usar el offset

	memcpy(MEM_FIS + offset, (uint8_t*) buffer, size);
}

/* Se ejecuta al recibir el del CPU
 */
uint8_t *solicitarBytes(uint32_t pid, uint32_t page, uint32_t offset, uint32_t size){

	uint8_t *frame = obtenerFrame(pid, page);
	if (frame == NULL)
		perror("No se pudo obtener el marco de la pagina. error");

	uint8_t *buffer = leerBytes(pid, frame, offset, size);
	if (buffer == NULL)
		perror("No se pudieron leer los bytes de la pagina. error");

	return buffer;
}

/* Esta es la funcion que en el TP viene a ser "Solicitar Bytes de una Pagina"
 */
uint8_t *leerBytes(uint32_t pid, uint32_t frame, uint32_t offset, uint32_t size){

	uint8_t *buffer = malloc(size);
	memcpy(buffer, (uint32_t) MEM_FIS + offset, size);

	return buffer;
}

/* Aplica funcion de hashing y encuentra el frame correcto del pid
 */
uint8_t *obtenerFrame(uint32_t pid, uint32_t page){

	return MEM_FIS; // por ahora este es el unico frame
}

//typedef enum {CPU=900, CONSOLA=213} tProceso;
