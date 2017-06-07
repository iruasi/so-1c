#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <tiposRecursos/tiposErrores.h>
#include <tiposRecursos/tiposPaquetes.h>

#include "manejadoresMem.h"
#include "manejadoresCache.h"
#include "apiMemoria.h"
#include "structsMem.h"
#include "auxiliaresMemoria.h"

#ifndef PID_MEM // es para la distinguir la Memoria de un PID cualquiera
#define PID_MEM 0
#endif

float retardo_mem; // latencia de acceso a Memoria Fisica
extern tMemoria *memoria;
extern tCacheEntrada *CACHE_lines;

// OPERACIONES DE LA MEMORIA

void retardo(int ms){
	retardo_mem = ms / 1000.0;
	printf("Se cambio la latencia de acceso a Memoria a %f segundos", retardo_mem);
}

void flush(void){

	int i;
	for (i = 0; i < memoria->entradas_cache; ++i){
		(CACHE_lines +i)->pid  = 0;
		(CACHE_lines +i)->page = 0;
	}
}

void dump(int pid){

	puts("COMIENZO DE DUMP");

	dumpCache();
	dumpMemStructs();
	dumpMemContent(pid);

	puts("FIN DEL DUMP");
}


void size(int pid, int *proc_size, int *mem_frs, int *mem_ocup, int *mem_free){

	if (pid < 0){
		fprintf(stderr, "Se intento pedir el tamanio de un proceso invalido\n");
		*proc_size = *mem_frs = *mem_ocup = *mem_free = MEM_EXCEPTION;
	}

	// inicializamos todas las variables en 0, luego distinguimos si pide tamanio de Memoria o de un proc
	*proc_size = *mem_frs = *mem_ocup = *mem_free = 0;

	if (pid == PID_MEM){
		*mem_frs  = memoria->marcos; // TODO:
		*mem_free = pageQuantity(PID_MEM);
		*mem_ocup = *mem_frs - *mem_free;

	} else {
		*proc_size = pageQuantity(pid);
	}
}




// API DE LA MEMORIA

int inicializarPrograma(int pid, int pageCount){

	int reservadas = reservarPaginas(pid, pageCount);
	if (reservadas == pageCount)
		puts("Se reservo bien la cantidad de paginas solicitadas");

	return 0;
}

int almacenarBytes(int pid, int page, int offset, int size, char *buffer){

	int stat;

	if ((stat = escribirBytes(pid, page, offset, size, buffer)) < 0){
		fprintf(stderr, "No se pudieron escribir los bytes a la pagina. stat: %d\n", stat);
		abortar(pid);
		return FALLO_ESCRITURA;
	}

	return 0;
}

char *solicitarBytes(int pid, int page, int offset, int size){
	printf("Se solicitan para el PID %d: %d bytes de la pagina %d\n", pid, size, page);

	char *buffer;
	if ((buffer = leerBytes(pid, page, offset, size)) == NULL){
		perror("No se pudieron leer los bytes de la pagina. error");
		abortar(pid);
		return NULL;
	}

	return buffer;
}

int asignarPaginas(int pid, int page_count){

	int stat;

	if((stat = reservarPaginas(pid, page_count)) != 0){
		fprintf(stderr, "No se pudieron reservar paginas para el proceso. error: %d", stat);
		abortar(pid);
	}

	printf("Se reservaron correctamente %d paginas", page_count);
	return 0;
}

/* Llamado por Kernel, libera una pagina de HEAP.
 * Retorna MEM_EXCEPTION si no puede liberar la pagina porque no existe o
 * porque simplemente no puede hacerse
 */
void liberarPagina(int pid, int page){
	printf("Se libera la pagina %d del PID %d\n", page, pid);
}

