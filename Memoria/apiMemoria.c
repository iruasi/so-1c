#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <tiposRecursos/tiposErrores.h>
#include <tiposRecursos/tiposPaquetes.h>

#include "manejadoresMem.h"
#include "apiMemoria.h"
#include "structsMem.h"
#include "auxiliaresMemoria.h"

extern tMemoria *memoria;
extern char *MEM_FIS;

// OPERACIONES DE LA MEMORIA

void flush(tCacheEntrada *cache, uint32_t quant){

	tCacheEntrada nullEntry = {0,0,0};

	int i;
	for (i = 0; i < quant; ++i)
		cache[i] = nullEntry;
}

void dump(void *mem_dir){ // de momento mem_dir no es nada

	void *dumpBuf = NULL;
	tHeapMeta *hmd = (tHeapMeta *) MEM_FIS;
	int i;

	puts("COMIENZO DE DUMP");
	for(i = 0; i < memoria->marco_size - SIZEOF_HMD; i += hmd->size + SIZEOF_HMD){

		if (!hmd->isFree){

			printf("Se muestran contenidos en la direccion de MEM_FIS %p:\n", hmd);

			dumpBuf = realloc(dumpBuf, hmd->size);
			memcpy(dumpBuf, (void *) (int) hmd + SIZEOF_HMD, hmd->size);
			puts(dumpBuf);
		}

		hmd = (void *) (int) hmd + hmd->size + SIZEOF_HMD;
	}

	puts("FIN DEL DUMP");
}


// API DE LA MEMORIA

int inicializarPrograma(int pid, int pageCount){

	int reservadas = reservarPaginas(pid, pageCount);
	if (reservadas == pageCount)
		puts("Se reservo bien la cantidad de paginas solicitadas");

	return 0;
}

uint32_t almacenarBytes(uint32_t pid, uint32_t page, uint32_t offset, uint32_t size, uint8_t* buffer){

	uint8_t *frame = obtenerFrame(pid, page);
	if (frame == NULL){
		perror("No se encontro el frame en memoria. error");
		return MEM_EXCEPTION;
	}

	//escribirBytes(pid, (uint32_t) *frame, offset, size, buffer);
	return 0;
}

char *solicitarBytes(uint32_t pid, uint32_t page, uint32_t offset, uint32_t size){

	// TODO:offset + size / memoria->marco_size --> cuantas paginas hay que pasar desde el 0

	uint8_t *frame = obtenerFrame(pid, page);
	if (frame == NULL)
		perror("No se pudo obtener el marco de la pagina. error");

	char *buffer = leerBytes(pid, (uint32_t) frame, offset, size);
	if (buffer == NULL)
		perror("No se pudieron leer los bytes de la pagina. error");

	return buffer;
}


void asignarPaginas(int pid, int page_count){

	int reserva;

	if((reserva = reservarPaginas(pid, page_count)) != 0){
		printf("No se pudieron reservar paginas para el proceso. error: %d", reserva);
		abortar(pid);
	}

	printf("Se reservaron correctamente %d paginas", reserva);
}




