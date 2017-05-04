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
#include "structsMem.h"

#define MAX_IP_LEN 16 // Este largo solo es util para IPv4
#define MAX_PORT_LEN 6

#define MAXMSJ 100 // largo maximo de mensajes a enviar. Solo utilizado para 1er checkpoint
#define MAX_SIZE 100

#define SIZEOF_HMD 5
#define ULTIMO_HMD 0x02 // valor hexa de 1 byte, se distingue de entre true y false
// tal vez no sea necesario que sea hexa

void* connection_handler(void *);
int recieve_and_deserialize(t_Package *package, int socketCliente);

uint8_t *setupMemoria(int quantity, int size);
tMemEntrada * setupCache(int quantity);
void wipeCache(tMemEntrada *cache, uint32_t quant);
uint8_t *buscarEnCache(uint32_t pid, uint32_t page);
bool entradaCoincide(tMemEntrada entrada, uint32_t pid, uint32_t page);
uint8_t *buscarEnMemoria(uint32_t pid, uint32_t page);
tMemEntrada crearEntrada(uint32_t pid, uint32_t page, uint32_t frame);

uint8_t *reservarBytes(int sizeReserva);
uint8_t esReservable(int size, tHeapMeta *heapMetaData);
tHeapMeta *crearNuevoHMD(uint8_t *dir_mem, int size);

uint8_t *obtenerFrame(uint32_t pid, uint32_t page);
uint8_t *buscarEnCache(uint32_t pid, uint32_t page);
void insertarEnCache(tMemEntrada entry);

uint32_t almacenarBytes(uint32_t pid, uint32_t page, uint32_t offset, uint32_t size, uint8_t* buffer);
void escribirBytes(uint32_t pid, uint32_t frame, uint32_t offset, uint32_t size, void* buffer); // todo: escribir implementacion

uint8_t *solicitarBytes(uint32_t pid, uint32_t page, uint32_t offset, uint32_t size);
uint8_t *leerBytes(uint32_t pid, uint32_t frame, uint32_t offset, uint32_t size);


int sizeFrame;

tMemoria *memoria;
tMemEntrada *CACHE;
uint8_t *MEM_FIS;

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

	// Para ver bien como es que funciona bien este printf, esta bueno meterse con el gdb y mirar la memoria de cerca..
	printf("bytes disponibles en MEM_FIS: %d\n MEM_FIS esta libre: %d\n",  * (uint16_t*) MEM_FIS, MEM_FIS[4]);


	//sv multihilo

	int sock_entrada , client_sock , clientSize , *new_sock;
	struct sockaddr_in client;
	clientSize = sizeof client;


	sock_entrada = makeListenSock(memoria->puerto_entrada);
	//Listen
	listen(sock_entrada , 3);

	//acepta y escucha comunicaiones
	puts("esperando comunicaciones entrantes...");

	while((client_sock = accept(sock_entrada, (struct sockaddr*) &client, (socklen_t*) &clientSize)))
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
	clearBuffer(buf, MAXMSJ);

	t_Package package;

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

int recieve_and_deserialize(t_Package *package, int socketCliente){

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


// FUNCIONES Y PROCEDIMIENTOS DE MANEJO DE MEMORIA

/* Retorna un puntero a un espacio de dir_mem donde se podran escribir bytes
 * Retorna NULL si no fue posible reservar espacio
 */
uint8_t *reservarBytes(int sizeReserva){
// por ahora trabaja con la unica pagina que existe

	tHeapMeta *hmd = (tHeapMeta *) MEM_FIS;
	int sizeLibre = sizeFrame;

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
 * Si es el ultimo HMD, retorna un valor #definido
 */
uint8_t esReservable(int size, tHeapMeta *hmd){

	if(! hmd->isFree || hmd->size - SIZEOF_HMD < size)
		return false;

	else if (hmd->size == 0) // esta libre y con espacio cero => es el ultimo MetaData
		return ULTIMO_HMD;

	return true;
}

/* Crea una cantidad quant de frames de tamanio size.
 * Se usa para la configuracion del espacio de memoria inicial
 */
uint8_t *setupMemoria(int quant, int size){

	uint8_t *frames = malloc(quant * size);

	// setteo del Heap MetaData
	tHeapMeta *hmd = malloc(sizeof hmd->isFree + sizeof hmd->size);
	hmd->isFree = true;
	hmd->size = size - SIZEOF_HMD;

	memcpy(frames, (uint8_t*) hmd, hmd->size);

	free(hmd);
	return frames;
}

/* 'Crea' una cantidad quant de frames de tamanio size.
 * Se usa para la configuracion del espacio de memoria inicial
 */
tMemEntrada *setupCache(int quantity){

	tMemEntrada *entradas = malloc(quantity * sizeof *entradas);
	wipeCache((uint8_t *) entradas, quantity);

	return entradas;
}

/* es parte del API de Memoria; Realiza el pedido de escritura de CPU
 */
uint32_t almacenarBytes(uint32_t pid, uint32_t page, uint32_t offset, uint32_t size, uint8_t* buffer){

	uint8_t *frame = obtenerFrame(pid, page);
	if (frame == NULL){
		perror("No se encontro el frame en memoria. error");
		return MEM_EXCEPTION;
	}

	escribirBytes(pid, (uint32_t) *frame, offset, size, buffer);
	return 0;
}


/* Esta es la funcion que en el TP viene a ser "Almacenar Bytes en una Pagina"
 */
void escribirBytes(uint32_t pid, uint32_t frame, uint32_t offset, uint32_t size, void* buffer){
// De momento tenemos una unica pagina, por lo que solo nos importa usar el offset

	memcpy(MEM_FIS + offset, (uint8_t*) buffer, size);
}

/* Se ejecuta al recibir el del CPU
 */
uint8_t *solicitarBytes(uint32_t pid, uint32_t page, uint32_t offset, uint32_t size){

	uint8_t *frame = obtenerFrame(pid, page);
	if (frame == NULL)
		perror("No se pudo obtener el marco de la pagina. error");

	uint8_t *buffer = leerBytes(pid, (uint32_t) frame, offset, size);
	if (buffer == NULL)
		perror("No se pudieron leer los bytes de la pagina. error");

	return buffer;
}

/* Esta es la funcion que en el TP viene a ser "Solicitar Bytes de una Pagina"
 */
uint8_t *leerBytes(uint32_t pid, uint32_t frame, uint32_t offset, uint32_t size){

	uint8_t *buffer = malloc(size);
	memcpy(buffer, (uint8_t *) ((uint32_t) MEM_FIS + offset), size);

	return buffer;
}

/* Busca el frame en cache o en la memoria principal
 */
uint8_t *obtenerFrame(uint32_t pid, uint32_t page){

	uint8_t *frame;

	if ((frame = buscarEnCache(pid, page)) != NULL)
		return frame;

	frame = buscarEnMemoria(pid, page);
	tMemEntrada new_entry = crearEntrada(pid, page, (uint32_t) frame);
	insertarEnCache(new_entry);

	return frame; // por ahora este es el unico frame
}

/* recorre secuencialmente CACHE hasta dar con una entrada que tenga el pid y page buscados
 */
uint8_t *buscarEnCache(uint32_t pid, uint32_t page){

	tMemEntrada *entradaCache = (tMemEntrada *) CACHE;

	int i;
	for (i = 0; i < memoria->entradas_cache; i++){
		if (entradaCoincide(entradaCache[i], pid, page))
			return (uint8_t *) entradaCache[i].frame;
	}

	return NULL;
}

/* obtiene la dir de una entrada reemplazable en cache y la pisa
 */
void insertarEnCache(tMemEntrada entry){

	tMemEntrada *entradaAPisar;//todo: algoritmoLRU();
	entradaAPisar = &entry;
}

/* nos dice si tenemos un cache hit o miss
 */
bool entradaCoincide(tMemEntrada entrada, uint32_t pid, uint32_t page){

	if (entrada.pid != pid || entrada.page != page)
		return false;

	return true;
}

/* crea una entrada de tipo tMemEntrada, util para la cache
 */
tMemEntrada crearEntrada(uint32_t pid, uint32_t page, uint32_t frame){

	tMemEntrada entry;
	entry.pid   = pid;
	entry.frame = frame;
	entry.page  = page;

	return entry;
}

/* limpia toda la cache
 */
void wipeCache(tMemEntrada *cache, uint32_t quant){

	tMemEntrada nullEntry = {0,0,0};

	int i;
	for (i = 0; i < quant; ++i)
		cache[i] = nullEntry;
}

/* mediante la funcion de hash encuentra el frame de la pagina de un proceso
 */
uint8_t *buscarEnMemoria(uint32_t pid, uint32_t page){

	uint8_t *frame = MEM_FIS; //hashFunction(pid, page); todo: investigar y ver como hacer una buena funcion de hashing

	return frame;
}

/* deshace la fragmentacion interna del HEAP dado en heap_dir
 */
void defragmentarHeap(uint8_t *heap_dir){
	//para este checkpoint, heap_dir va a ser exactamente MEM_FIS

	uint8_t *head = heap_dir;

	tHeapMeta *head_hmd = (tHeapMeta *) head;
	uint32_t off = 0;

	bool rta = esReservable(0, head_hmd);
	int compact = 0;

	while (rta != ULTIMO_HMD){
		if (rta == true){

			off += head_hmd->size + SIZEOF_HMD;
			compact++;
			rta = esReservable(0, (tHeapMeta *) off);

		} else if (compact) { // no es un bloque reservable, pero es compactable

			printf("Se compactan %d bloques de memoria..\n", compact);
			head_hmd->size = ((uint32_t) head_hmd) + off - SIZEOF_HMD;
			compact = 0;
			head_hmd = (tHeapMeta *) off;

		} else { // no es un bloque reservable, ni es compactable
			head_hmd += head_hmd->size + SIZEOF_HMD;
		}

		rta = esReservable(0, head_hmd);
		off = 0;
	}
}
