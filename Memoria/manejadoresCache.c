#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <tiposRecursos/tiposErrores.h>

#include "structsMem.h"
#include "manejadoresCache.h"
#include "apiMemoria.h"
#include "memoriaConfigurators.h"

extern char *CACHE; // memoria CACHE
extern int *CACHE_accs;      // vector de accesos a CACHE
tCacheEntrada *CACHE_lines; // vector de lineas a CACHE
extern char* MEM_FIS;
extern tMemoria *memoria;    // configuracion de Memoria
extern int pid_free; // definiciones para paginas de Memoria, Tabla de Invertidas y paginas libres

int setupCache(void){

	if ((CACHE = malloc(memoria->entradas_cache * memoria->marco_size)) == NULL){
		fprintf(stderr, "No se pudo crear espacio de memoria para Memoria de CACHE");
		return FALLO_GRAL;
	}

	if ((CACHE_lines = malloc(memoria->entradas_cache * sizeof *CACHE_lines)) == NULL){
		fprintf(stderr, "No se pudo crear espacio de memoria para Lineas de CACHE");
		return FALLO_GRAL;
	}
	setupCacheLines();

	if ((CACHE_accs = calloc(memoria->entradas_cache, sizeof *CACHE_accs)) == NULL){
		fprintf(stderr, "No se pudo crear espacio de memoria para Accesos de CACHE");
		return FALLO_GRAL;
	}

	return 0;
}

void setupCacheLines(void){

	int off, i;
	for(i = off = 0; i < memoria->entradas_cache; i++){
		(CACHE_lines + i)->dir_cont = CACHE + off;
		off += memoria->marco_size;
	}

	flush();
}

char *getCacheContent(int pid, int page){
	puts("Buscando contenido en CACHE...");

	int i;
	for (i = 0; i < memoria->entradas_cache; ++i){
		if (CACHE_lines[i].pid == pid && CACHE_lines[i].page == page){
			puts("CACHE hit");
			cachePenalizer(i);
			return CACHE_lines[i].dir_cont;
		}
	}

	puts("CACHE miss");
	return NULL;
}

void actualizarCache(int pid, int page, int frame){

	int i;
	char *mem_cont = MEM_FIS + frame * memoria->marco_size;

	for (i = 0; i < memoria->entradas_cache; ++i){
		if ((CACHE_lines + i)->pid == pid && (CACHE_lines + i)->page == page){
			puts("Se encontro el frame a actualizar en cache. Writeback...");
			memcpy((CACHE_lines + i)->dir_cont, mem_cont, memoria->marco_size);
			return;
		}
	}
}

void cachePenalizer(int accessed){

	int i;
	for (i = 0; i < memoria->entradas_cache; ++i)
		CACHE_accs[i]--;
	CACHE_accs[accessed] = 0;
}

int insertarEnCache(int pid, int page, char *cont){

	int min_pos;

	if (pageCachedQuantity(pid) >= memoria->cache_x_proc){
		printf("El proceso %d tiene la maxima cantidad de entradas en CACHE permitidas\nNo se inserta entrada nueva\n", pid);
		return MAX_CACHE_ENTRIES;
	}

	tCacheEntrada *entry = getCacheVictim(&min_pos);
	cachePenalizer(min_pos);

	entry->pid  = pid;
	entry->page = page;
	memcpy(entry->dir_cont, cont, memoria->marco_size);

	return 0;
}

tCacheEntrada *getCacheVictim(int *min_line){

	int i;
	for (i = *min_line = 0; i < memoria->entradas_cache; ++i){

		if (CACHE_lines->pid == pid_free) // entrada libre
			return CACHE_lines + *min_line;

		(CACHE_accs[i] < *min_line)? *min_line = i : *min_line;
	}

	return CACHE_lines + *min_line;
}

int pageCachedQuantity(int pid){

	int i, pages;
	for (i = pages = 0; i < memoria->entradas_cache; ++i){
		(CACHE_lines[i].pid == pid)? pages++ : pages;
	}

	return pages;
}

void dumpCache(void){

	int i;
	puts("Comienzo dump Cache");

	printf("PID \t\t PAGINA");
	for (i = 0; i < memoria->entradas_cache; ++i)
		printf("%d \t\t %d\n", (CACHE_lines + i)->pid, (CACHE_lines + i)->page);

	puts("Fin dump Cache");
}
