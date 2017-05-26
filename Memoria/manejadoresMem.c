#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "manejadoresMem.h"
#include "structsMem.h"
#include "memoriaConfigurators.h"
#include "apiMemoria.h"
#include "../Compartidas/tiposErrores.h"

extern int size_frame;
extern int quant_frames;

tMemoria *memoria;
tCacheEntrada *CACHE;
uint8_t *MEM_FIS;
extern char *mem_ptr;

// FUNCIONES Y PROCEDIMIENTOS DE MANEJO DE MEMORIA

uint8_t *reservarBytes(int sizeReserva){
// por ahora trabaja con la unica pagina que existe

	tHeapMeta *hmd = (tHeapMeta *) MEM_FIS;
	int sizeLibre = size_frame - 5;

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


int setupMemoria(int frames, int frame_size, char (**mem_ptr)[frames][frame_size]){

	*mem_ptr = malloc(sizeof (char[frames][frame_size]));
	if (mem_ptr == NULL){
		perror("No se pudo crear el espacio de Memoria. error");
		return MEM_EXCEPTION;
	}
	int i,j;
	for (i = 0; i < 3; i++){
		for (j = 0; j < 4; j++)
			(**mem_ptr)[i][j] = 'T';
	}

	return 0;
}


tCacheEntrada *setupCache(int quantity){

	tCacheEntrada *entradas = malloc(quantity * sizeof *entradas);
	flush(entradas, quantity);

	return entradas;
}

void escribirBytes(uint32_t pid, uint32_t frame, uint32_t offset, uint32_t size, void* buffer){

	int j;

	for (j = 0; j < 12; j++){
		printf("%c ", mem_ptr[j]);
		((j +1 ) % 4 == 0)? puts("") : '\0' ;
	}

	char (*n)[quant_frames][size_frame];
	n = mem_ptr;

	puts("metodo posta");
	int i;
	for(i = 0; i < 3 ; i++){
		for (j = 0; j < 4; j++)
			printf("%c ", (*n)[i][j]);
		puts("");
	}

	puts("hecho");

	memcpy(MEM_FIS + offset, (uint8_t*) buffer, size);
}

uint8_t *leerBytes(uint32_t pid, uint32_t frame, uint32_t offset, uint32_t size){

	uint8_t *buffer = malloc(size);
	memcpy(buffer, (uint8_t *) ((uint32_t) MEM_FIS + offset), size);

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

	tCacheEntrada *entradaCache = (tCacheEntrada *) CACHE;

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

uint8_t *buscarEnMemoria(uint32_t pid, uint32_t page){

	uint8_t *frame = MEM_FIS; //hashFunction(pid, page); todo: investigar y ver como hacer una buena funcion de hashing

	return frame;
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
