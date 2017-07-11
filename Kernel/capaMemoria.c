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

#ifndef VALID_ALLOC
#define VALID_ALLOC(S) (((S) > 0 && (S) <= MAX_ALLOC_SIZE)? true : false)
#endif

extern int MAX_ALLOC_SIZE;

t_dictionary *heapDict; // pid_string(char*) : heap_pages(t_list)
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

uint8_t esReservable(int size, tHeapMeta *hmd){

	if(! hmd->isFree || hmd->size - SIZEOF_HMD < size)
		return false;

	else if (hmd->size == 0) // esta libre y con espacio cero => es el ultimo MetaData
		return ULTIMO_HMD;

	return true;
}

void crearNuevoHMD(char *dir_mem, int size){

	tHeapMeta *new_hmd = (tHeapMeta *) dir_mem;
	new_hmd->size = size;
	new_hmd->isFree = true;
}

t_puntero reservarBytes(char* heap_page, int sizeReserva){ // todo: convertir retorno a t_puntero

	int sizeLibre = frame_size - SIZEOF_HMD;
	char *next_hmd = heap_page;
	tHeapMeta *hmd = (tHeapMeta*) next_hmd;

	uint8_t rta = esReservable(sizeReserva, hmd);
	while(rta != ULTIMO_HMD){
		hmd = (tHeapMeta *) next_hmd;
		next_hmd = (char *) hmd + SIZEOF_HMD + hmd->size;

		if (rta == true){

			hmd->size = sizeReserva;
			hmd->isFree = false;

			sizeLibre -= sizeReserva;
			crearNuevoHMD(next_hmd, sizeLibre);

			return (char *) hmd + SIZEOF_HMD;
		}

		sizeLibre -= hmd->size;
		rta = esReservable(sizeReserva, (tHeapMeta *) next_hmd);
	}

	return NULL;
}

t_puntero reservar(int pid, int size){

	int stat, pag_h;

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

	return reservarEnHeap(pid, size, &pag_h);
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

t_puntero reservarEnHeap(int pid, int size, int *pag){
	char spid[MAXPID_DIG];
	sprintf(spid, "%d", pid);

	int i;
	t_puntero ptr;
	char *heap;
	t_list *heaps_pid;

	heaps_pid = dictionary_get(heapDict, spid);
	for (i = 0; i < list_size(heaps_pid); ++i){

		pag = list_get(heaps_pid, i);
		heap = obtenerHeapDeMemoria(pid, *pag);

		if ((ptr = (t_puntero) reservarBytes(heap, size))){
			escribirEnMemoria(pid, *pag, heap);
			return ptr;
		}
		freeAndNULL((void **) &heap);
	}

	printf("No se encontro espacio en Heaps del PID %d para %d bytes\n", pid, size);
	puts("Pedimos una nueva pagina..");

	crearNuevoHeap(pid);
	heap = obtenerHeapDeMemoria(pid, *pag);
	if ( !(ptr = (t_puntero) reservarBytes(heap, size)) ){
		printf("Fue imposible allocar %d bytes para PID %d en un Heap nuevo\n", size, pid);
		return MEM_EXCEPTION;
	}

	escribirEnMemoria(pid, *pag, heap);
	return ptr;
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

	agregarHeapAPID(heap_pp->pid, heap_pp->pageCount);
	printf("Se asigno la pagina %d al proceso %d", heap_pp->pageCount, heap_pp->pid);

	free(heap_pp);
	free(buffer);
	return 0;
}

void liberar(t_puntero ptr){

	// obtener pagina de Memoria
	// si ptr es puntero a hmd notFree
	// marcar como libre, revisar a derecha

}

void enviarPrimerHMD(int pid, int pag){
puts("Le asignamos el HMD inicial a la pagina de Heap...");

	int pack_size, stat;
	tPackByteAlmac fst_hmd;
	char *hmd_serial;
	tHeapMeta hmd = {.isFree = true, .size = MAX_ALLOC_SIZE};

	fst_hmd.head.tipo_de_proceso = KER; fst_hmd.head.tipo_de_mensaje = BYTES;
	fst_hmd.pid    = pid; fst_hmd.page = pag;
	fst_hmd.offset = 0;	  fst_hmd.size = SIZEOF_HMD;
	fst_hmd.bytes = malloc(SIZEOF_HMD);
	memcpy(fst_hmd.bytes, &hmd, SIZEOF_HMD);

	hmd_serial = serializeByteAlmacenamiento(&fst_hmd, &pack_size);

	if ((stat = send(sock_mem, hmd_serial, pack_size, 0)) == -1){
		perror("Fallo envio del primer HMD a Memoria. error");
		return;
	}
	printf("Se enviaron %d bytes a Memoria\n", stat);

	free(fst_hmd.bytes);
	free(hmd_serial);
}

void agregarHeapAPID(int pid, int pag){ // todo: verificar que se agrega el nro de pag correcto
	char spid[MAXPID_DIG];
	sprintf(spid, "%d", pid);
	printf("Registramos la pagina de heap %d al PID %s\n", pag, spid);

	int *pag_v = malloc(sizeof(int));
	    *pag_v = pid; // copio por valor a un nuevo ptr, habra que free'arlo

	    if (!tieneHeap(pid)){
		t_list *heaps = list_create();
		dictionary_put(heapDict, spid, heaps);
	}
	t_list *heaps = dictionary_get(heapDict, spid);
	list_add(heaps, pag_v);
}


bool tieneHeap(int pid){

	char spid[MAXPID_DIG];
	sprintf(spid, "%d", pid);
	return dictionary_has_key(heapDict, spid);
}



int reservarPagHeap(int pid, int size_reserva){

	int stat;
	char *pidpag_serial;

	tPackPidPag *pidpag;
	if ((pidpag = malloc(sizeof *pidpag)) == NULL){
		fprintf(stderr, "No se pudo crear espacio de memoria para paquete pid_paginas\n");
		return FALLO_GRAL;
	}
	pidpag->head.tipo_de_proceso = KER;
	pidpag->head.tipo_de_mensaje = ASIGN_PAG;
	pidpag->pid = pid;
	pidpag->pageCount = 1;
	int pack_size;
	if ((pidpag_serial = serializePIDPaginas(pidpag, &pack_size)) == NULL){
		fprintf(stderr, "Fallo el serializado de PID y Paginas\n");
		return FALLO_SERIALIZAC;
	}

	if ((stat = send(sock_mem, pidpag_serial, sizeof *pidpag, 0)) == -1){
		perror("Fallo el envio de pid y paginas serializado. error");
		return FALLO_SEND;
	}

	freeAndNULL((void **) &pidpag);
	freeAndNULL((void **) &pidpag_serial);
	return 0;
}
