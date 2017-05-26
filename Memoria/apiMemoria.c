#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../Compartidas/tiposErrores.h"
#include "../Compartidas/tiposPaquetes.h"
#include "manejadoresMem.h"
#include "apiMemoria.h"
#include "structsMem.h"


extern int size_frame;
extern uint8_t *MEM_FIS;

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
	for(i = 0; i < size_frame - SIZEOF_HMD; i += hmd->size + SIZEOF_HMD){

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

void *inicializarPrograma(int pid, int pageCount){

	return NULL;
}

// todo: esto no tiene que ser asi, en realidad...
uint8_t *inicializarProgramaBeta(int pid, int pageCount, int sourceSize, void *srcCode){
// para este checkpoint pasado, tenemos que almacenar el source_code en MEM_FIS

	tSegmentosProg segsProg;

	void *espacio_en_mem = reservarBytes(sourceSize);

	printf("Copiando %d bytes en %p...\n", sourceSize, espacio_en_mem);
	memcpy((void *) (int) espacio_en_mem + SIZEOF_HMD, srcCode, sourceSize);
	puts("Hecho!\n");

	return NULL;
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

uint8_t *solicitarBytes(uint32_t pid, uint32_t page, uint32_t offset, uint32_t size){

	uint8_t *frame = obtenerFrame(pid, page);
	if (frame == NULL)
		perror("No se pudo obtener el marco de la pagina. error");

	uint8_t *buffer = leerBytes(pid, (uint32_t) frame, offset, size);
	if (buffer == NULL)
		perror("No se pudieron leer los bytes de la pagina. error");

	return buffer;
}

uint8_t *asignarPaginas(int pid, int page_count){
	return NULL;
}
