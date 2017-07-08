#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#include "manejadoresMem.h"
#include "manejadoresCache.h"
#include "auxiliaresMemoria.h"
#include "structsMem.h"
#include "memoriaConfigurators.h"
#include "apiMemoria.h"

#include <funcionesCompartidas/funcionesCompartidas.h>
#include <tiposRecursos/tiposErrores.h>

int pid_free, pid_inv, free_page;
int marcos_inv; // cantidad de frames que ocupa la tabla de invertidas en MEM_FIS

extern float retardo_mem;   // latencia de acceso a Memoria Fisica
extern tMemoria *memoria;   // configuracion de Memoria
extern char *MEM_FIS;       // MEMORIA FISICA
extern char *CACHE;         // memoria CACHE
extern int *CACHE_accs;     // vector de accesos a CACHE
tCacheEntrada *CACHE_lines; // vector de lineas a CACHE


// FUNCIONES Y PROCEDIMIENTOS DE MANEJO DE MEMORIA


void liberarEstructurasMemoria(void){

	liberarConfiguracionMemoria();
	freeAndNULL((void **) &MEM_FIS);
	freeAndNULL((void **) &CACHE);
	freeAndNULL((void **) &CACHE_lines);
	freeAndNULL((void **) &CACHE_accs);
}

int setupMemoria(void){
	int stat;

	retardo(memoria->retardo_memoria);

	pid_free   = -2;
	pid_inv   = -3;
	free_page = -1;

	if ((MEM_FIS = malloc(memoria->marcos * memoria->marco_size)) == NULL){
		puts("No se pudo crear el espacio de Memoria");
		return MEM_EXCEPTION;
	}
	populateInvertidas();

	// inicializamos la "CACHE"
	if ((stat = setupCache()) != 0)
		return MEM_EXCEPTION;

	return 0;
}

void populateInvertidas(void){

	int i, off, fr;

	tEntradaInv entry_inv  = {.pid = pid_inv,  .pag = free_page};
	tEntradaInv entry_free = {.pid = pid_free, .pag = free_page};

	int size_inv_total = sizeof(tEntradaInv) * memoria->marcos;
	marcos_inv = ceil((float) size_inv_total / memoria->marco_size);


	// creamos las primeras entradas administrativas, una por cada `marco_invertido'
	for(i = off = fr = 0; i < marcos_inv; i ++){
		memcpy(MEM_FIS + fr * memoria->marco_size + off, &entry_inv, sizeof(tEntradaInv));
		nextFrameValue(&fr, &off, sizeof(tEntradaInv));
	}

	// creamos las otras entradas libres, una por cada marco restante
	for(i = marcos_inv; i < memoria->marcos; i++){
		memcpy(MEM_FIS + fr * memoria->marco_size + off, &entry_free, sizeof(tEntradaInv));
		nextFrameValue(&fr, &off, sizeof(tEntradaInv));
	}
}

char *leerBytes(int pid, int page, int offset, int size){

	char *cont = NULL;

	char *buffer;
	if ((buffer = malloc(size + 1)) == NULL){
		puts("No se pudo crear espacio de memoria para un buffer");
		return NULL;
	}

	if ((cont = getCacheContent(pid, page)) == NULL){
		if ((cont = getMemContent(pid, page)) == NULL){
			puts("No pudo obtener el frame");
			return NULL;
		}
		insertarEnCache(pid, page, cont);
	}

	memcpy(buffer, cont + offset, size + 1);
	buffer[size] = '\0';
	return buffer;
}

char *getMemContent(int pid, int page){

	int frame;
	if ((frame = buscarEnMemoria(pid, page)) < 0){
		printf("Fallo buscar En Memoria el pid %d y pagina %d; \tError: %d\n", pid, page, frame);
		return NULL;
	}

	return MEM_FIS + frame * memoria->marco_size;
}

int buscarEnMemoria(int pid, int page){
	sleep(retardo_mem);

	tEntradaInv *entry;
	int cic, off, fr; // frame y offset para recorrer la tabla de invertidas
	int frame_ap = frameHash(pid, page); // aproximacion de frame buscado

	for(cic = 0; cic < memoria->marcos; cic++){

		gotoFrameInvertida(frame_ap, &fr, &off);
		entry = (tEntradaInv *) (MEM_FIS + fr * memoria->marco_size + off);

		if (pid == entry->pid && page == entry->pag)
			return frame_ap;
		frame_ap = (frame_ap+1) % memoria->marcos;
	}

	return FRAME_NOT_FOUND;
}

int frameHash(int pid, int page){
	char str1[20];
	char str2[20];
	sprintf(str1, "%d", pid);
	sprintf(str2, "%d", page);
	strcat(str1, str2);
	int frame_apr = atoi(str1) % memoria->marcos;
	return frame_apr;
}

void dumpMemStructs(void){ // todo: revisar

	int i, fr, off;
	tEntradaInv *entry;

	puts("Comienzo dump Tabla de Paginas Invertida");
	printf("PID \t\t PAGINA");
	for (i = fr = off = 0; i < marcos_inv; ++i){
		entry = (tEntradaInv*) (MEM_FIS + fr * memoria->marco_size + off);
		printf("%d \t\t %d\n", entry->pid, entry->pag);
		nextFrameValue(&fr, &off, sizeof(tEntradaInv));
	}
	puts("Fin dump Tabla de Paginas Invertida");

	puts("Comienzo dump Listado Procesos Activos");
	// todo: dumpear listado de procesos activos... No se ni lo que es un proc activo
	puts("Fin dump Listado Procesos Activos");

}

void dumpMemContent(int pid){

	if (pid == -4){
		puts("Se muestra info de todos los procesos de Memoria: (no implementado aun)");
		return;
	}

	int pag;
	char *cont;
	int page_count = pageQuantity(pid);

	printf("PID \t\t PAGINA \t\t CONTENIDO");
	for (pag = 0; pag < page_count; ++pag){
		cont = getMemContent(pid, pag);
		printf("%d \t\t %d \t\t %s\n", pid, pag, cont);
	}
}
