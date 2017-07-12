#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <semaphore.h>

#include <parser/parser.h>
#include <commons/collections/dictionary.h>

#include <tiposRecursos/tiposErrores.h>
#include <tiposRecursos/tiposPaquetes.h>
#include <funcionesPaquetes/funcionesPaquetes.h>
#include <funcionesCompartidas/funcionesCompartidas.h>

#include "capaMemoria.h"
#include "defsKernel.h"

extern int MAX_ALLOC_SIZE;

t_dictionary *heapDict; // pid_string(char*) : heap_pages(t_list)-> tHeapProc(int, int)
sem_t sem_heapDict;
sem_t sem_bytes;
sem_t sem_end_exec;

//extern int frames;
extern int frame_size;
extern int sock_mem;

void setupHeapStructs(void){

	MAX_ALLOC_SIZE = frame_size - 2 * SIZEOF_HMD;
	heapDict = dictionary_create();

	sem_init(&sem_heapDict, 0, 0);
	sem_init(&sem_bytes, 0, 0);
	sem_init(&sem_end_exec, 0, 0);
}

bool esReservable(int size_req, tHeapMeta *hmd){

	if(! hmd->isFree || hmd->size < size_req)
		return false;

	return true;
}

void crearNuevoHMD(tHeapMeta *dir_mem, int size){

	dir_mem->size = size;
	dir_mem->isFree = true;
}

/* retorna posicion relativa la pagina de heap, NO ES ABSOLUTA
 */
t_puntero reservarBytes(char* heap_page, int sizeReserva){
	printf("Se tratan de reservar %d bytes en pagina de Heap\n", sizeReserva);

	int sizeLibre = MAX_ALLOC_SIZE;
	int dist      = MAX_ALLOC_SIZE + SIZEOF_HMD;
	tHeapMeta *hmd, *hmd_next;

	hmd = (tHeapMeta *) heap_page;
	while(sizeLibre >= sizeReserva){

		if (esReservable(sizeReserva, hmd)){

			hmd->size = sizeReserva;
			hmd->isFree = false;

			sizeLibre -= sizeReserva;
			hmd_next = nextBlock(hmd, &dist);

			crearNuevoHMD(hmd_next, sizeLibre);
			return ((char *) hmd - heap_page) + SIZEOF_HMD; // posicion relativa
		}

		sizeLibre -= hmd->size;
		hmd = nextBlock(hmd, &dist);
	}

	return 0;
}

t_puntero reservar(int pid, int size){
	printf("Se reservaran %d bytes para el PID %d\n", size, pid);

	int stat;
	t_puntero ptr;

	if (!VALID_ALLOC(size)){
		printf("El size %d no es un tamanio valido para almacenar en Memoria\n", size);
		return 0; // Un puntero a Heap nunca podria ser 0
	}

	if (!tieneHeap(pid)){
		if ((stat = crearNuevoHeap(pid)) != 0){
			puts("No se pudo crear pagina de heap!");
			return 0;
		}
	}

	if ( !(ptr = reservarEnHeap(pid, size)) )
		if ( !(ptr = intentarReservaUnica(pid, size)) )
			return 0;

	return ptr;
}

t_puntero intentarReservaUnica(int pid, int size){

	t_puntero  ptr;
	int stat;
	t_list    *heaps;
	tHeapProc *hp;
	char      *heap, spid[MAXPID_DIG];

	if ((stat = crearNuevoHeap(pid)) != 0){
		puts("No se pudo crear pagina de heap!");
		return 0;
	}

	sprintf(spid, "%d", pid);
	heaps    = dictionary_get(heapDict, spid);
	hp = list_get(heaps, list_size(heaps)-1);

	if ((heap = obtenerHeapDeMemoria(pid, hp->page)) == NULL)
		return 0;

	if ( !(ptr = reservarBytes(heap, size)) ){
		printf("No se pudieron reservar %d bytes en la pagina %d del PID %d\n", size, hp->page, pid);
		return 0;
	}

	hp->max_size = getMaxFreeBlock(heap);
	escribirEnMemoria(pid, hp->page, heap);
	return ptr + hp->page * frame_size;
}


int escribirEnMemoria(int pid, int pag, char *heap){

	int stat, pack_size;
	tPackByteAlmac byteal;
	char *h_serial;

	byteal.head.tipo_de_proceso = KER; byteal.head.tipo_de_mensaje = ALMAC_BYTES;
	byteal.pid    = pid; byteal.page = pag;
	byteal.offset = 0;   byteal.size = frame_size;
	byteal.bytes  = heap;

	if ((h_serial = serializeByteAlmacenamiento(&byteal, &pack_size)) == NULL){
		puts("No se pudo serializar el heap para Memoria");
		return FALLO_SERIALIZAC;
	}

	if ((stat = send(sock_mem, h_serial, pack_size, 0)) == -1){
		perror("Fallo send de heap serial a Memoria. error");
		return FALLO_SEND;
	}
	printf("Se enviaron los %d bytes de heap a Memoria\n", stat);

	freeAndNULL((void **) &heap);
	free(h_serial);
	return 0;
}

t_puntero reservarEnHeap(int pid, int size){
	char spid[MAXPID_DIG];
	sprintf(spid, "%d", pid);

	int i;
	t_puntero ptr;
	char *heap;
	t_list *heaps_pid;
	tHeapProc *hp;

	heaps_pid = dictionary_get(heapDict, spid);
	for (i = 0; i < list_size(heaps_pid); ++i){
		hp = list_get(heaps_pid, i);

		if (size > hp->max_size)
			continue;

		heap = obtenerHeapDeMemoria(pid, hp->page);

		if ((ptr = reservarBytes(heap, size))){
			hp->max_size = getMaxFreeBlock(heap);
			escribirEnMemoria(pid, hp->page, heap);
			return ptr + hp->page * frame_size; // posicion absoluta de Memoria
		}

		freeAndNULL((void **) &heap);
	}

	return 0;
}

/* Retorna el bloque contiguo al dado por parametro, si es que hay uno...
 */
tHeapMeta *nextBlock(tHeapMeta *hmd, int *dist){

	if (ES_ULTIMO_HMD(hmd, *dist)){
		*dist = 0;
		return hmd;
	}

	*dist -= hmd->size + SIZEOF_HMD;
	return (tHeapMeta* ) ((char *) hmd + hmd->size + SIZEOF_HMD);
}

/* Avanza sobre el heap hasta encontrar el proximo bloque libre.
 * El maximo puede llegar es hasta el ULTIMO_HMD, el cual siempre es free.
 * Retorna el bloque encontrado.
 */
tHeapMeta *nextFreeBlock(tHeapMeta *hmd, int *dist){

	tHeapMeta *hmd_next = nextBlock(hmd, dist);
	while (!hmd_next->isFree)
		hmd_next = nextBlock(hmd_next, dist);

	return hmd_next;
}


int getMaxFreeBlock(char *heap){

	tHeapMeta *hmd = (tHeapMeta *) heap;
	int dist = MAX_ALLOC_SIZE + SIZEOF_HMD;
	int max  = 0;

	if (!hmd->isFree)
		hmd = nextFreeBlock(hmd, &dist);

	while (dist){
		max = MAX(max, hmd->size);
		hmd = nextFreeBlock(hmd, &dist);
	}

	return max;
}

char *obtenerHeapDeMemoria(int pid, int pag){

	tPackByteReq byterq;
	tPackBytes *bytes;
	char *byterq_serial, *buff_bytes;
	int stat, pack_size;

	char *heap = malloc(frame_size);

	byterq.head.tipo_de_proceso = KER; byterq.head.tipo_de_mensaje = BYTES;
	byterq.pid    = pid; byterq.page = pag;
	byterq.offset = 0;   byterq.size = frame_size;

	if ((byterq_serial = serializeByteRequest(&byterq, &pack_size)) == NULL){
		puts("Fallo serializacion de pedido de heap");
		return NULL;
	}

	if ((stat = send(sock_mem, byterq_serial, pack_size, 0)) == -1){
		perror("Fallo envio de pedido de bytes a Memoria. error");
		return NULL;
	}

	sem_wait(&sem_bytes);
	if ((buff_bytes = recvGeneric(sock_mem)) == NULL){
		puts("Fallo recepcion generica");
		return NULL;
	}
	sem_post(&sem_end_exec);
	if ((bytes = deserializeBytes(buff_bytes)) == NULL){
		puts("Fallo deserializacion de Bytes");
		return NULL;
	}
	memcpy(heap, bytes->bytes, frame_size);

	free(bytes->bytes); free(bytes);
	free(byterq_serial);
	free(buff_bytes);
	return heap;
}

/* Pide a Memoria una nueva pagina que se usara como Heap.
 * Le asigna a dicha pagina el primer HMD.
 * Retorna 0 en caso exitoso, valor negativo en caso de fallo
 */
int crearNuevoHeap(int pid){

	int stat, pack_size;
	tPackPidPag *heap_pp;
	char *buffer;

	heap_pp = malloc(sizeof *heap_pp);
	heap_pp->head.tipo_de_proceso = KER; heap_pp->head.tipo_de_mensaje = ASIGN_PAG;
	heap_pp->pid = pid;
	heap_pp->pageCount = 1;

	if ((buffer = serializePIDPaginas(heap_pp, &pack_size)) == NULL){
		puts("No se pudo serializar pedido de pagina Heap");
		return FALLO_SERIALIZAC;
	}
	freeAndNULL((void **) &heap_pp);

	if ((stat = send(sock_mem, buffer, pack_size, 0)) == -1){
		perror("Fallo send pedido de pagina a Memoria. error");
		return FALLO_SEND;
	}
	freeAndNULL((void **) &buffer);

	sem_wait(&sem_heapDict);
	buffer = recvGeneric(sock_mem);
	sem_post(&sem_end_exec);
	heap_pp = deserializePIDPaginas(buffer);

	if (heap_pp->pageCount < 0){
		puts("No se pudo asignar una pagina en Memoria para el Heap");
		return FALLO_ASIGN;
	}

	agregarHeapAPID(heap_pp->pid, heap_pp->pageCount);


	free(heap_pp);
	free(buffer);
	return 0;
}

void liberar(t_puntero ptr){

	// obtener pagina de Memoria
	// si ptr es puntero a hmd notFree
	// marcar como libre, revisar a derecha

}

void agregarHeapAPID(int pid, int pag){
	char spid[MAXPID_DIG];
	sprintf(spid, "%d", pid);
	printf("Registramos la pagina de heap %d al PID %s\n", pag, spid);

	tHeapProc *hp = malloc(sizeof *hp);
	hp->page = pag; hp->max_size = MAX_ALLOC_SIZE;

	if (!tieneHeap(pid)){
		t_list *heaps = list_create();
		dictionary_put(heapDict, spid, heaps);
	}

	t_list *heaps = dictionary_get(heapDict, spid);
	list_add(heaps, hp);
}

bool tieneHeap(int pid){

	char spid[MAXPID_DIG];
	sprintf(spid, "%d", pid);
	return dictionary_has_key(heapDict, spid);
}
