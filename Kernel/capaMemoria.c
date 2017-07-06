#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#include <parser/parser.h>

#include <tiposRecursos/tiposErrores.h>
#include <tiposRecursos/tiposPaquetes.h>
#include <funcionesPaquetes/funcionesPaquetes.h>
#include <funcionesCompartidas/funcionesCompartidas.h>

#include "capaMemoria.h"

extern int MAX_ALLOC_SIZE;

extern tHeapProc *hProcs;
extern int hProcs_cant;
//extern int frames;
extern int frame_size;

void actualizarHProcsConPagina(int pid, int new_heapPage){

	int i;
	if (new_heapPage != 0){ // no es pagina de codigo

	for (i = 0; i < hProcs_cant ; ++i){
		if ((hProcs +i)->pid == pid)
			(hProcs +i)->pag_heap_cant++;

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


