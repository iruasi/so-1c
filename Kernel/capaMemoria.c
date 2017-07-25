#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <semaphore.h>
#include <pthread.h>

#include <parser/parser.h>
#include <commons/collections/dictionary.h>
#include <commons/log.h>
#include <tiposRecursos/tiposErrores.h>
#include <tiposRecursos/tiposPaquetes.h>
#include <funcionesPaquetes/funcionesPaquetes.h>
#include <funcionesCompartidas/funcionesCompartidas.h>

#include "capaMemoria.h"
#include "defsKernel.h"
#include "auxiliaresKernel.h"

t_dictionary *dict_heap; // pid_string(char*) : heap_pages(t_list)-> tHeapProc(int, int)
pthread_mutex_t mux_dict_heap;
extern pthread_mutex_t mux_infoProc;
sem_t sem_heapDict, sem_bytes, sem_end_exec;

int MAX_ALLOC_SIZE;
extern int frame_size;
extern int sock_mem;
extern t_log * logger;

void setupGlobales_capaMemoria(void){

	dict_heap      = dictionary_create();

	sem_init(&sem_heapDict, 0, 0);
	sem_init(&sem_bytes,    0, 0);
	sem_init(&sem_end_exec, 0, 0);

	pthread_mutex_init(&mux_dict_heap,  NULL);
}

bool esReservable(int size_req, tHeapMeta *hmd){

	if(! hmd->isFree || hmd->size < size_req)
		return false;

	return true;
}

void crearNuevoHMD(tHeapMeta *dir_mem, int size){

	dir_mem->size = size - SIZEOF_HMD;
	dir_mem->isFree = true;
}

/* retorna posicion relativa la pagina de heap, NO ES ABSOLUTA
 */
t_puntero reservarBytes(char* heap_page, int sizeReserva){
	log_trace(logger, "Se tratan de reservar %d bytes en pagina de Heap\n", sizeReserva);

	int dist = MAX_ALLOC_SIZE + SIZEOF_HMD;
	tHeapMeta *hmd, *hmd_next;

	hmd = (tHeapMeta *) heap_page;
	while(dist >= sizeReserva){

		if (esReservable(sizeReserva, hmd)){

			hmd->size = sizeReserva;
			hmd->isFree = false;

			hmd_next = nextBlock(hmd, &dist);

			crearNuevoHMD(hmd_next, dist);
			return ((char *) hmd - heap_page) + SIZEOF_HMD; // posicion relativa
		}

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

	MUX_LOCK(&mux_infoProc);
	sumarSizeYAlloc(pid, size);
	MUX_UNLOCK(&mux_infoProc);
	return ptr;
}

t_puntero intentarReservaUnica(int pid, int size){
	char *heap, spid[MAXPID_DIG];
	sprintf(spid, "%d", pid);

	t_puntero  ptr;
	int stat;
	t_list    *heaps;
	tHeapProc *hp;

	if ((stat = crearNuevoHeap(pid)) != 0){
		puts("No se pudo crear pagina de heap!");
		return 0;
	}

	MUX_LOCK(&mux_dict_heap);
	heaps = dictionary_get(dict_heap, spid);
	MUX_UNLOCK(&mux_dict_heap);

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

	MUX_LOCK(&mux_dict_heap);
	heaps_pid = dictionary_get(dict_heap, spid);
	MUX_UNLOCK(&mux_dict_heap);

	for (i = 0; i < list_size(heaps_pid); ++i){
		hp = list_get(heaps_pid, i);

		if (size > hp->max_size)
			continue;

		if ((heap = obtenerHeapDeMemoria(pid, hp->page)) == NULL)
			return 0;

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
	MUX_LOCK(&mux_infoProc);
	sumarPaginaHeap(pid);
	MUX_UNLOCK(&mux_infoProc);

	free(heap_pp);
	free(buffer);
	return 0;
}

int liberar(int pid, t_puntero ptr){
	printf("Se liberara el puntero %d para el PID %d\n", ptr, pid);
	char spid[MAXPID_DIG];
	sprintf(spid, "%d", pid);

	char *heap;
	tHeapMeta *hmd;
	int pag = ptr / frame_size;
	int off = ptr % frame_size - 5;
	tHeapProc *hp;

	if (!paginaPerteneceAPID(spid, pag, &hp)){
		printf("No se encontro para el programa %s la pagina %d\n", spid, pag);
		return FALLO_HEAP;
	}

	if ((heap = obtenerHeapDeMemoria(pid, hp->page)) == NULL)
		return FALLO_SOLIC;

	if (!punteroApuntaABloqueValido(heap, ptr))
		return PUNTERO_INVALIDO;

	hmd = (tHeapMeta *) (heap + off);
	hmd->isFree = true; // liberamos el bloque
	MUX_LOCK(&mux_infoProc);
	sumarFreeYDealloc(pid, hmd->size);
	MUX_UNLOCK(&mux_infoProc);
	consolidar(heap);
	hp->max_size = getMaxFreeBlock(heap);

	if ((escribirEnMemoria(pid, hp->page, heap)) != 0)
		return FALLO_ALMAC;

	return 0;
}

bool paginaPerteneceAPID(char *spid, int pag, tHeapProc **hp){

	int i;
	t_list *heaps = dictionary_get(dict_heap, spid);

	for (i = 0; i < list_size(heaps); ++i){
		*hp = list_get(heaps, i);
		if ((*hp)->page == pag)
			return true;
	}
	return false;
}

bool punteroApuntaABloqueValido(char *heap, t_puntero ptr){

	tHeapMeta *hmd;
	// off apuntaria 5 bytes despues del comienzo del HMD, corregimos eso...
	int off = ptr % frame_size - SIZEOF_HMD;

	if (!punteroApuntaABloque(heap, ptr))
		return false;

	if ((hmd = (tHeapMeta *) (heap + off))->isFree){
		printf("Puntero %d con off %d apunta a bloque Heap ya libre\n", ptr, off);
		return false;
	}

	return true;
}

bool punteroApuntaABloque(char *heap, t_puntero ptr){

	tHeapMeta *hmd = (tHeapMeta *) heap;
	int dist = MAX_ALLOC_SIZE + SIZEOF_HMD;

	// off apuntaria 5 bytes despues del comienzo del HMD, corregimos eso...
	int off = ptr % frame_size - 5;

	while(dist){
		if ((char *) hmd == heap + off)
			return true;
		hmd = nextBlock(hmd, &dist);
	}
	printf("Puntero %d con off %d no apunta a ningun bloque Heap\n", ptr, off);
	return false;
}

/* Trata de consolidar bloques libres contiguos del Heap.
 */
void consolidar(char *heap){

	tHeapMeta *hmd, *hmd_n;
	int dist = MAX_ALLOC_SIZE + SIZEOF_HMD;
	int dist_n;

	hmd = (tHeapMeta *) heap;
	if (!hmd->isFree)
		hmd = nextFreeBlock((tHeapMeta *) heap, &dist);
	if (!dist)
		return;

	dist_n = dist;
	hmd_n  = nextFreeBlock(hmd, &dist_n);
	while(dist_n){

		if ((char *) hmd_n != (char *) hmd + hmd->size + SIZEOF_HMD){ // no son contiguos
			hmd   = hmd_n;
			hmd_n = nextFreeBlock(hmd, &dist_n);
			continue;

		} else{ // son contiguos, cosolidamos y `limpiamos' hmd_n
			printf("Encontro bloques consolidables size %d y %d, size final: %d\n",
					hmd->size, hmd_n->size, hmd->size + hmd_n->size + SIZEOF_HMD);
			hmd->size += hmd_n->size + SIZEOF_HMD;
			hmd_n = nextFreeBlock(hmd_n, &dist_n);
		}
	}
}

void agregarHeapAPID(int pid, int pag){
	char spid[MAXPID_DIG];
	sprintf(spid, "%d", pid);
	printf("Registramos la pagina de heap %d al PID %s\n", pag, spid);

	tHeapProc *hp = malloc(sizeof *hp);
	hp->page = pag; hp->max_size = MAX_ALLOC_SIZE;

	if (!tieneHeap(pid)){
		MUX_LOCK(&mux_infoProc);
		t_infoProcess *ip = getInfoProcessByPID(pid);
		MUX_UNLOCK(&mux_infoProc);
		ip->ih = malloc(sizeof *ip->ih);

		t_list *heaps = list_create();
		MUX_LOCK(&mux_dict_heap);
		dictionary_put(dict_heap, spid, heaps);
		MUX_UNLOCK(&mux_dict_heap);
	}

	t_list *heaps = dictionary_get(dict_heap, spid);
	list_add(heaps, hp);
}

void liberarHeapEnKernel(int pid){
	char spid[MAXPID_DIG];
	sprintf(spid, "%d", pid);
	printf("Eliminamos los registros de Heap del PID %s\n", spid);

	if (!tieneHeap(pid))
		return;

	MUX_LOCK(&mux_dict_heap);
	t_list *heaps = dictionary_remove(dict_heap, spid);
	MUX_UNLOCK(&mux_dict_heap);

	while (list_size(heaps))
		free(list_remove(heaps, 0));
}

bool tieneHeap(int pid){
	char spid[MAXPID_DIG];
	sprintf(spid, "%d", pid);

	MUX_LOCK(&mux_dict_heap);
	bool rta = dictionary_has_key(dict_heap, spid);
	MUX_UNLOCK(&mux_dict_heap);

	return rta;
}
