#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <commons/log.h>

#include "manejadoresMem.h"
#include "manejadoresCache.h"
#include "auxiliaresMemoria.h"
#include "structsMem.h"
#include "memoriaConfigurators.h"
#include "apiMemoria.h"

#include <tiposRecursos/tiposErrores.h>



int marcos_inv; // cantidad de frames que ocupa la tabla de invertidas en MEM_FIS

extern float retardo_mem;      // latencia de acceso a Memoria Fisica
extern tMemoria *memoria;    // configuracion de Memoria
extern char *MEM_FIS;        // MEMORIA FISICA

// FUNCIONES Y PROCEDIMIENTOS DE MANEJO DE MEMORIA


/* Esta deberia ser la rutina para destruir cualquier programa en forma abortiva
 */
void abortar(int pid){ // TODO: escribir el comportamiento de esta funcion
	log_info(logger,"Se inicio procedimiento de aborto");
	log_info(logger,"Matamos todo lo que le pertenece al PID: %d",pid);
}


// Se sabe previamente que el frame corresponde a uno de HEAP
char *reservarBytes(int pid, int heap_page, int sizeReserva){
// por ahora trabaja con la unica pagina que existe

	tHeapMeta *hmd = (tHeapMeta *) MEM_FIS;
	int sizeLibre = memoria->marco_size - SIZEOF_HMD;

	uint8_t rta = esReservable(sizeReserva, hmd);
	while(rta != ULTIMO_HMD){

		if (rta == true){

			hmd->size = sizeReserva;
			hmd->isFree = false;

			sizeLibre -= sizeReserva;
			uint8_t* dirNew_hmd = (uint8_t *) ((uint32_t) hmd + SIZEOF_HMD + hmd->size);
			crearNuevoHMD(dirNew_hmd, sizeLibre);

			return (char*) hmd;
		}

		sizeLibre -= hmd->size;

		uint8_t* dir = (uint8_t *) ((uint32_t) hmd + SIZEOF_HMD + hmd->size);
		hmd = (tHeapMeta *) dir;
		rta = esReservable(sizeReserva, hmd);
	}

	return NULL; // en esta unica pagina no tuvimos espacio para reservar sizeReserva
}


tHeapMeta *crearNuevoHMD(uint8_t *dir_mem, int size){

	tHeapMeta *new_hmd = (tHeapMeta *) dir_mem;
	new_hmd->size = size;
	new_hmd->isFree = true;

	return new_hmd;
}

uint8_t esReservable(int size, tHeapMeta *hmd){

	if(! hmd->isFree || hmd->size - SIZEOF_HMD < size)
		return false;

	else if (hmd->size == 0) // esta libre y con espacio cero => es el ultimo MetaData
		return ULTIMO_HMD;

	return true;
}


int setupMemoria(void){ // todo: cuando termina el proceso Memoria hay que liberar

	MEM_FIS = malloc(memoria->marcos * memoria->marco_size);
	if (MEM_FIS == NULL){
		log_error(logger,"No se pudo crear el espacio de Memoria");
		return MEM_EXCEPTION;
	}

	populateInvertidas();
	return 0;
}

void populateInvertidas(void){

	int i, off, fr;

	tEntradaInv entry_inv  = {.pid = -1, .pag = -1};
	tEntradaInv entry_free = {.pid =  0, .pag = -1};

	int size_inv_total = sizeof(tEntradaInv) * memoria->marcos;
	marcos_inv = ceil((float) size_inv_total / memoria->marco_size);


	// creamos las primeras entradas administrativas, una por cada `marco_invertido'
	for(i = off = fr = 0; i < marcos_inv; i ++){
		memcpy(MEM_FIS + fr * memoria->marco_size + off, &entry_inv, marcos_inv);
		nextFrameValue(&fr, &off, sizeof(tEntradaInv));
	}

	// creamos las otras entradas libres, una por cada marco restante
	for(i = marcos_inv; i < memoria->marcos; i++){
		memcpy(MEM_FIS + fr * memoria->marco_size + off, &entry_free, marcos_inv);
		nextFrameValue(&fr, &off, sizeof(tEntradaInv));
	}
}


int escribirBytes(int pid, int frame, int offset, int size, void* buffer){

	memcpy(MEM_FIS + (frame * memoria->marco_size) + offset, buffer, size);
	return 0;
}

char *leerBytes(int pid, int page, int offset, int size){

	int frame;
	char *cont = NULL;

	char *buffer;
	if ((buffer = malloc(size)) == NULL){
		log_error(logger,"No se pudo crear espacio de memoria para un buffer");
		return NULL;
	}

	if ((cont = getCacheContent(pid, page)) != NULL){
		memcpy(buffer, cont + offset, size);
		return buffer;

	} else if ((frame = getMemFrame(pid, page)) < 0){
		log_error(logger,"No se pudo obtener el frame");
		return NULL;
	}

	cont = getMemContent(frame, offset);
	insertarEnCache(pid, page, cont);

	memcpy(buffer, cont, size);
	return buffer;
}

int getMemFrame(int pid, int page){
	log_info(logger,"Buscando contenido en Memoria");


	int frame;

	if ((frame = buscarEnMemoria(pid, page)) < 0){
		log_error(logger,"No se encontro la pagina %d para el pid %d en Memoria",page,pid);
		return FRAME_NOT_FOUND;
	}

	sleep(retardo_mem);
	return frame;
}

char *getMemContent(int frame, int offset){
	return MEM_FIS + frame * memoria->marco_size + offset;
}

int buscarEnMemoria(int pid, int page){
// por ahora la busqueda es secuencial...

	int i, off, fr; // frame y offset para recorrer la tabla de invertidas
	gotoFrameInvertida(marcos_inv, &fr, &off);

	int frame_repr = marcos_inv + 1; // frame representativo de la posicion en MEMORIA FISICA posta

	tEntradaInv *entry = (tEntradaInv *) MEM_FIS;
	for(i = 0; i < marcos_inv; frame_repr++){
		gotoFrameInvertida(frame_repr, &fr, &off);
		entry = (tEntradaInv *) (MEM_FIS + fr * memoria->marco_size + off);
		if (pid == entry->pid && page == entry->pag)
			return frame_repr;
	}

	//uint8_t *frame = MEM_FIS; //hashFunction(pid, page); // todo: investigar y ver como hacer una buena funcion de hashing

	return FRAME_NOT_FOUND;
}

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


			log_info(logger,"Se compactan %d bloques de memoria..",compact);
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

void dumpMemStructs(void){

	int i, fr, off;
	tEntradaInv *entry;
	log_info(logger,"Comienzo dump Tabla de Paginas Invertida");
	log_info(logger,"ID \t\t PAGINA");

	for (i = fr = off = 0; i < marcos_inv; ++i){
		entry = (tEntradaInv*) (MEM_FIS + fr * memoria->marco_size + off);
		log_info(logger,"%d \t\t %d\n", entry->pid, entry->pag);

		nextFrameValue(&fr, &off, sizeof(tEntradaInv));
	}
	log_info(logger,"Fin dump Tabla de Paginas Invertida");
	log_info(logger,"Comienzo dump Listado Procesos Activos");
	// todo: dumpear listado de procesos activos... No se ni lo que es un proc activo
	log_info(logger,"Fin dump Listado Procesos Activos");

}

void dumpMemContent(int pid){

	if (pid == 0){
		log_info(logger,"Se muestra info de todos los procesos de Memoria: (no implementado aun)");
	} else {

	int pag, frame;
	char *cont;
	int page_count = pageQuantity(pid);

	log_info(logger,"PID \t\t PAGINA \t\t CONTENIDO");
	for (pag = 0; pag < page_count; ++pag){
		frame = buscarEnMemoria(pid, pag);
		cont = getMemContent(frame, 0);
		log_info(logger,"%d \t\t %d \t\t %s\n", pid, pag, cont);
	}

	}
}





