#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#include <parser/parser.h>

#include <tiposRecursos/tiposErrores.h>
#include <tiposRecursos/tiposPaquetes.h>
#include <funcionesPaquetes/funcionesPaquetes.h>
#include <funcionesCompartidas/funcionesCompartidas.h>

#include "capaMemoria.h"


char *serializeByteRequestByPack(tPackByteReq *bytereq);
tPackByteReq *crearByteRequestHeap(tPackHeader head, int pid, int page_heap);

extern int MAX_ALLOC_SIZE;
extern tHeapProc *heapsPorProc;
extern int hProcs_cant;
//extern int frames;
extern int frame_size;
extern int sock_mem;

/* Para un pid y nro de pagina, pide la pagina a Memoria mediante un send
 * En escencia es un caso especifico de Solicitud de Bytes.
 * La funcion que se complementa con esta es recibirPaginaHeap();
 */
int pedirPaginaHeap(int sock_memoria, int pid, int page){

	int stat;
	tPackHeader head = {.tipo_de_proceso = KER, .tipo_de_mensaje = SOLIC_BYTES};
	tPackByteReq *byterq = crearByteRequestHeap(head, pid, page);

	char *byterq_serial = serializeByteRequestByPack(byterq);

	if((stat = send(sock_memoria, byterq_serial, sizeof(tPackByteReq), 0)) == -1){
		puts("Fallo envio del pedido de pagina Heap");
		return FALLO_SEND;
	}
	return stat;
}

/* Recibiriamos una pagina, pero deberiamos asegurarnos de asociarla al pid y
 * numero de pagina correspondientes
 * todo: struct {int pid, int page, char[frame_size] page_cont}tPagina;
 */
char *recibirPaginaHeap(int sock_memoria){

	int stat;
	char *pag_heap = calloc(1, frame_size);

	if ((stat = recv(sock_memoria, pag_heap, frame_size, 0)) == -1){
		puts("Fallo la recepcion de pagina de heap");
		return NULL;
	}

	return pag_heap;
}

void actualizarHProcsConPagina(int pid, int new_heapPage){

	int i;
	if (new_heapPage != 0){ // no es pagina de codigo

	for (i = 0; i < hProcs_cant ; ++i){
		if ((heapsPorProc +i)->pid == pid)
			(heapsPorProc +i)->pag_heap_cant++;

	}}
}

int inicializarHMD(int pid, int pagenum){

	return pagenum;
}

t_puntero allocar(int pid, int size){

	t_puntero p = NULL;
	//reservarPagHeap();
	//obtenerPagHeap();
	//reservarBytes();
	return p;
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

char *reservarBytes(char* heap_page, int sizeReserva){

	int sizeLibre = frame_size - SIZEOF_HMD;
	char *hmd = heap_page;

	uint8_t rta = esReservable(sizeReserva, (tHeapMeta*) hmd);
	while(rta != ULTIMO_HMD){

		if (rta == true){

			((tHeapMeta*) hmd)->size = sizeReserva;
			((tHeapMeta*) hmd)->isFree = false;

			sizeLibre -= sizeReserva;
			char *dirNew_hmd = hmd + SIZEOF_HMD + ((tHeapMeta*) hmd)->size;
			crearNuevoHMD(dirNew_hmd, sizeLibre);

			return hmd + SIZEOF_HMD;
		}

		sizeLibre -= ((tHeapMeta*) hmd)->size;

		hmd = hmd + SIZEOF_HMD + ((tHeapMeta*) hmd)->size;
		rta = esReservable(sizeReserva, (tHeapMeta*) hmd);

	}

	return NULL; // en esta unica pagina no tuvimos espacio para reservar sizeReserva
}


int reservarPagHeap(int sock_mem, int pid, int size_reserva){

	int stat;
	char *pidpag_serial;

	if (size_reserva > MAX_ALLOC_SIZE){
		fprintf(stderr, "Trato de allocar %d bytes. Maximo permitido: %d\n", MAX_ALLOC_SIZE, size_reserva);
		return MEM_OVERALLOC;
	}

	tPackPidPag *pidpag;
	if ((pidpag = malloc(sizeof *pidpag)) == NULL){
		fprintf(stderr, "No se pudo crear espacio de memoria para paquete pid_paginas\n");
		return FALLO_GRAL;
	}
	pidpag->head.tipo_de_proceso = KER;
	pidpag->head.tipo_de_mensaje = ASIGN_PAG;
	pidpag->pid = pid;
	pidpag->pageCount = 1;

	if ((pidpag_serial = serializePIDPaginas(pidpag)) == NULL){
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


// todo: esto esta repetido en cpu.c, depues mandarlo a funcionesPaquetes...
char *serializeByteRequestByPack(tPackByteReq *bytereq){

	int off;
	char *bytereq_serial;
	if ((bytereq_serial = malloc(24)) == NULL){ // todo: rompe aca
		fprintf(stderr, "No se pudo mallocar espacio para el paquete de pedido de bytes\n");
		return NULL;
	}

	off = 0;
	memcpy(bytereq_serial+off, &bytereq->head, HEAD_SIZE);
	off += HEAD_SIZE;
	memcpy(bytereq_serial + off, &bytereq->pid, sizeof bytereq->pid);
	off += sizeof bytereq->pid;
	memcpy(bytereq_serial + off, &bytereq->page, sizeof bytereq->page);
	off += sizeof bytereq->page;
	memcpy(bytereq_serial + off, &bytereq->offset, sizeof bytereq->offset);
	off += sizeof bytereq->offset;
	memcpy(bytereq_serial, &bytereq->size, sizeof bytereq->size);
	off += sizeof bytereq->size;

	return bytereq_serial;
}

tPackByteReq *crearByteRequestHeap(tPackHeader head, int pid, int page_heap){

	int off = 0;
	tPackByteReq *bytereq = malloc(sizeof(tPackByteReq));

	memcpy(&bytereq->head, &head, HEAD_SIZE);
	memcpy(&bytereq->pid, &pid, sizeof pid);
	bytereq->page = page_heap;
	memcpy(&bytereq->offset, &off, sizeof (int));
	memcpy(&bytereq->size, &frame_size, sizeof (int));

	return bytereq;
}
