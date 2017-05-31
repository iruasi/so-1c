#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#include "manejadoresMem.h"
#include "auxiliaresMemoria.h"
#include "structsMem.h"
#include "memoriaConfigurators.h"
#include "apiMemoria.h"
#include "../Compartidas/tiposErrores.h"

int size_inv_total;
int marcos_inv; // cantidad de frames que ocupa la tabla de invertidas en MEM_FIS
extern tMemoria *memoria;
extern tCacheEntrada *CACHE;
extern char *MEM_FIS;

// FUNCIONES Y PROCEDIMIENTOS DE MANEJO DE MEMORIA

uint8_t *reservarBytes(int sizeReserva){
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

			return (uint8_t *) hmd;
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


int setupMemoria(int frames, int frame_size){

	MEM_FIS = malloc(frames * frame_size);
	if (MEM_FIS == NULL){
		perror("No se pudo crear el espacio de Memoria. error");
		return MEM_EXCEPTION;
	}

	//populateInvertidas(frames, frame_size);

	return 0;
}

void populateInvertidas(int frames, int frame_size){

	int i, off, fr;

	tEntradaInv templ = {.pid = -1, .pag = -1};
	tEntradaInv *invpag_tab = malloc(sizeof *invpag_tab);

	size_inv_total = sizeof(tEntradaInv) * frames;
	marcos_inv = ceil((float) size_inv_total / memoria->marcos);

	for(i = off = fr = 0; i < frames; ++i){
		nextFrameValue(&fr, &off, sizeof(tEntradaInv));
		memcpy((void *) invpag_tab + fr * frame_size + off, &templ, marcos_inv);
	}
}

int setupCache(int quantity){

	CACHE = malloc(quantity * sizeof *CACHE);
	if (CACHE == NULL){
		perror("No se pudo crear el espacio de Cache. error");
		return MEM_EXCEPTION;
	}

	flush(CACHE, quantity);
	return 0;
}

void escribirBytes(uint32_t pid, uint32_t frame, uint32_t offset, uint32_t size, void* buffer){

	memcpy(MEM_FIS + (frame * memoria->marco_size) + offset, buffer, size);
}

uint8_t *leerBytes(uint32_t pid, uint32_t frame, uint32_t offset, uint32_t size){
	int size_real = size + 1;
	uint8_t *buffer = malloc(size_real);
	memcpy(buffer, MEM_FIS + (frame * memoria->marco_size) + offset, size_real);

	return buffer;
}

uint8_t *obtenerFrame(uint32_t pid, uint32_t page){

	uint8_t *frame;

	if ((frame = buscarEnCache(pid, page)) != NULL)
		return frame;

	frame = buscarEnMemoria(pid, page);
	tCacheEntrada new_entry = crearEntrada(pid, page, (uint32_t) frame);
	insertarEnCache(new_entry);

	return frame; // por ahora este es el unico frame
}

uint8_t *buscarEnCache(uint32_t pid, uint32_t page){

	tCacheEntrada *entradaCache = CACHE;

	int i;
	for (i = 0; i < memoria->entradas_cache; i++){
		if (entradaCoincide(entradaCache[i], pid, page))
			return (uint8_t *) entradaCache[i].frame;
	}

	return NULL;
}

void insertarEnCache(tCacheEntrada entry){

	tCacheEntrada *entradaAPisar;//todo: algoritmoLRU();
	entradaAPisar = &entry;
}

bool entradaCoincide(tCacheEntrada entrada, uint32_t pid, uint32_t page){

	if (entrada.pid != pid || entrada.page != page)
		return false;

	return true;
}

tCacheEntrada crearEntrada(uint32_t pid, uint32_t page, uint32_t frame){

	tCacheEntrada entry;
	entry.pid   = pid;
	entry.frame = frame;
	entry.page  = page;

	return entry;
}

int buscarEnMemoria(uint32_t pid, uint32_t page){ // todo: investigar y ver como hacer una buena funcion de hashing
// por ahora la busqueda es secuencial...

	// frame para recorrer la tabla de invertidas, comienza salteando los frames que sabemos son de la propia tabla invertida
	int fr = MEM_FIS + marcos_inv * sizeof(tEntradaInv);
	int i, off; // offset para recorrer la tabla de invertidas

	int frame_repr = marcos_inv + 1; // frame representativo de la posicion en MEMORIA FISICA posta
	gotoFrameInvertida(marcos_inv, &fr, &off);

	tEntradaInv *entry = (tEntradaInv *) MEM_FIS;
	for(i = 0; i < marcos_inv; frame_repr++){
		gotoFrameInvertida(frame_repr, &fr, &off);
		entry = (tEntradaInv *) (MEM_FIS + fr * memoria->marco_size + off);
		if (pid == entry->pid && page == entry->pag)
			return frame_repr;
	}

	//inv_marcos;

	//uint8_t *frame = MEM_FIS; //hashFunction(pid, page);

	return 0;// todo:
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
