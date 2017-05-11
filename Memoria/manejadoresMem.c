#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "manejadoresMem.h"
#include "structsMem.h"
#include "memoriaConfigurators.h"
#include "../Compartidas/tiposErrores.h"

#define SIZEOF_HMD 5
#define ULTIMO_HMD 0x02 // valor hexa de 1 byte, se distingue de entre true y false

int sizeFrame;
tMemoria *memoria;
tMemEntrada *CACHE;
uint8_t *MEM_FIS;

// FUNCIONES Y PROCEDIMIENTOS DE MANEJO DE MEMORIA

uint8_t *inicializarPrograma(int pid, int pageCount, int sourceSize, void *srcCode){
// para este checkpoint pasado, tenemos que almacenar el source_code en MEM_FIS

	printf("inicializarPrograma! Recibimos pid %d y la cantidad de paginas a reservar es de %d\n", pid, pageCount);

	int offset = SIZEOF_HMD;
	printf("Copiando %d bytes en MEM_FIS...\n", sourceSize);
	memcpy((uint8_t *) ((uint32_t) MEM_FIS + offset), srcCode, sourceSize);
	puts("Hecho!\n");

	return NULL;
}


uint8_t *asignarPaginas(int pid, int page_count){
	return NULL;
}


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


tMemEntrada *setupCache(int quantity){

	tMemEntrada *entradas = malloc(quantity * sizeof *entradas);
	wipeCache(entradas, quantity);

	return entradas;
}

uint32_t almacenarBytes(uint32_t pid, uint32_t page, uint32_t offset, uint32_t size, uint8_t* buffer){

	uint8_t *frame = obtenerFrame(pid, page);
	if (frame == NULL){
		perror("No se encontro el frame en memoria. error");
		return MEM_EXCEPTION;
	}

	escribirBytes(pid, (uint32_t) *frame, offset, size, buffer);
	return 0;
}


void escribirBytes(uint32_t pid, uint32_t frame, uint32_t offset, uint32_t size, void* buffer){
// De momento tenemos una unica pagina, por lo que solo nos importa usar el offset

	memcpy(MEM_FIS + offset, (uint8_t*) buffer, size);
}

uint8_t *solicitarBytes(uint32_t pid, uint32_t page, uint32_t offset, uint32_t size){

	uint8_t *frame = obtenerFrame(pid, page);
	if (frame == NULL)
		perror("No se pudo obtener el marco de la pagina. error");

	uint8_t *buffer = leerBytes(pid, (uint32_t) frame, offset, size);
	if (buffer == NULL)
		perror("No se pudieron leer los bytes de la pagina. error");

	return buffer;
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
	tMemEntrada new_entry = crearEntrada(pid, page, (uint32_t) frame);
	insertarEnCache(new_entry);

	return frame; // por ahora este es el unico frame
}

uint8_t *buscarEnCache(uint32_t pid, uint32_t page){

	tMemEntrada *entradaCache = (tMemEntrada *) CACHE;

	int i;
	for (i = 0; i < memoria->entradas_cache; i++){
		if (entradaCoincide(entradaCache[i], pid, page))
			return (uint8_t *) entradaCache[i].frame;
	}

	return NULL;
}

void insertarEnCache(tMemEntrada entry){

	tMemEntrada *entradaAPisar;//todo: algoritmoLRU();
	entradaAPisar = &entry;
}

bool entradaCoincide(tMemEntrada entrada, uint32_t pid, uint32_t page){

	if (entrada.pid != pid || entrada.page != page)
		return false;

	return true;
}

tMemEntrada crearEntrada(uint32_t pid, uint32_t page, uint32_t frame){

	tMemEntrada entry;
	entry.pid   = pid;
	entry.frame = frame;
	entry.page  = page;

	return entry;
}

void wipeCache(tMemEntrada *cache, uint32_t quant){

	tMemEntrada nullEntry = {0,0,0};

	int i;
	for (i = 0; i < quant; ++i)
		cache[i] = nullEntry;
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
