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

float retardo_mem; // latencia de acceso a Memoria Fisica
extern tMemoria *memoria;
extern tCacheEntrada *CACHE_lines;
extern int pid_free, free_page;

// OPERACIONES DE LA MEMORIA

void retardo(int ms){
	retardo_mem = ms / 1000.0;
	printf("Se cambio la latencia de acceso a Memoria a %f segundos", retardo_mem);
}

void flush(void){

	int i;
	for (i = 0; i < memoria->entradas_cache; ++i){
		(CACHE_lines +i)->pid  = pid_free;
		(CACHE_lines +i)->page = free_page;
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

	if (pid == pid_free){
		*mem_frs  = memoria->marcos; // TODO:
		*mem_free = pageQuantity(pid_free);
		*mem_ocup = *mem_frs - *mem_free;

	} else {
		*proc_size = pageQuantity(pid);
	}
}




// API DE LA MEMORIA

int inicializarPrograma(int pid, int pageCount){
	puts("Se inicializa un programa");

	int reservadas = reservarPaginas(pid, pageCount);
	if (reservadas >= 0)
		puts("Se reservo bien la cantidad de paginas solicitadas");

	return 0;
}

int almacenarBytes(int pid, int page, int offset, int size, char *buffer){

	int frame;

	if ((frame = buscarEnMemoria(pid, page)) < 0){
		printf("Fallo buscar En Memoria el pid %d y pagina %d; \tError: %d\n", pid, page, frame);
		return frame;
	}

	memcpy(MEM_FIS + frame * memoria->marco_size + offset, buffer, size);
	actualizarCache(pid, page, frame);

	return 0;
}

char *solicitarBytes(int pid, int page, int offset, int size){
	printf("Se solicitan para el PID %d: %d bytes de la pagina %d\n", pid, size, page);

	char *buffer;
	if ((buffer = leerBytes(pid, page, offset, size)) == NULL){
		puts("No se pudieron leer los bytes de la pagina");
		abortar(pid);
		return NULL;
	}

	return buffer;
}

int asignarPaginas(int pid, int page_count){

	int new_page;

	if((new_page = reservarPaginas(pid, page_count)) != 0){
		fprintf(stderr, "No se pudieron reservar paginas para el proceso. error: %d\n", new_page);
		abortar(pid);
	}

	printf("Se reservaron correctamente %d paginas\n", page_count);
	return new_page;
}

/* Llamado por Kernel, libera una pagina de HEAP.
 * Retorna MEM_EXCEPTION si no puede liberar la pagina porque no existe o
 * porque simplemente no puede hacerse
 */
void liberarPagina(int pid, int page){
	printf("Se libera la pagina %d del PID %d\n", page, pid);
}

