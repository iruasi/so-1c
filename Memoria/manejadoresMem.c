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



int marcos_inv; // cantidad de frames que ocupa la tabla de invertidas en MEM_FIS

extern float retardo_mem;   // latencia de acceso a Memoria Fisica
extern tMemoria *memoria;   // configuracion de Memoria
extern char *MEM_FIS;       // MEMORIA FISICA
extern char *CACHE;         // memoria CACHE
extern int *CACHE_accs;     // vector de accesos a CACHE
tCacheEntrada *CACHE_lines; // vector de lineas a CACHE


// FUNCIONES Y PROCEDIMIENTOS DE MANEJO DE MEMORIA


/* Esta deberia ser la rutina para destruir cualquier programa en forma abortiva
 */
void abortar(int pid){ // TODO: escribir el comportamiento de esta funcion
	puts("Se inicio procedimiento de aborto");
	printf("Matamos todo lo que le pertenece al PID: %d\n", pid);
}





void liberarEstructurasMemoria(void){

	liberarConfiguracionMemoria();
	freeAndNULL((void **) &MEM_FIS);
	freeAndNULL((void **) &CACHE);
	freeAndNULL((void **) &CACHE_lines);
	freeAndNULL((void **) &CACHE_accs);

}

int setupMemoria(void){

	MEM_FIS = malloc(memoria->marcos * memoria->marco_size);
	if (MEM_FIS == NULL){
		perror("No se pudo crear el espacio de Memoria. error");
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
		fprintf(stderr, "No se pudo crear espacio de memoria para un buffer\n");
		return NULL;
	}

	if ((cont = getCacheContent(pid, page)) != NULL){
		memcpy(buffer, cont + offset, size);
		return buffer;

	} else if ((frame = getMemFrame(pid, page)) < 0){
		fprintf(stderr, "No pudo obtener el frame\n");
		return NULL;
	}

	cont = getMemContent(frame, offset);
	insertarEnCache(pid, page, cont);

	memcpy(buffer, cont, size);
	return buffer;
}

int getMemFrame(int pid, int page){
	puts("Buscando contenido en Memoria");

	int frame;

	if ((frame = buscarEnMemoria(pid, page)) < 0){
		fprintf(stderr, "No se encontro la pagina %d para el pid %d en Memoria\n", page, pid);
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

	/* Logica
	 * buscar (pid , page){
	 * 	dirPid = hashFunc(pid , page); guardamos el valor de la tabla
	 * 	valorDirPid = traerValor(dirPid); trae el valor PID que esta en la posicion de la tabla
	 * 	while (pid != valorDirPid){
	 * 		dirPid = traerSiguiente(dirPid); trae la direccion siguiente a esa posicion del campo de la tabla
	 * 		if( dirPid == NULL)
	 * 			return FRAME_NOT_FOUND; // si no esta agregado en esa cadena de memoria, arroja este error
	 * 		valorDirPid =  traerValor(dirPid);
	 * 	return dirPid;
	 *
	 */



	//uint8_t *frame = MEM_FIS; //hashFunction(pid, page);

	return FRAME_NOT_FOUND;
}

int funcionHash(int pid, int page){

	int direccion = pid % marcos_inv;

	if ((direccion + page) > marcos_inv)

		direccion = (direccion + page) % marcos_inv;

	else

		direccion = (direccion + page);

	return direccion;
}

int agregarEnMemoria (int pid, int page){

	int *dirPidAnterior, *dirPid;
	dirPidAnterior = malloc(sizeof(int));
	dirPid = malloc(sizeof(int));
	*dirPid = funcionHash(pid, page);


	if (buscarEnMemoria(pid, page) == FRAME_NOT_FOUND){

		int valorDirPid = traerValor(dirPid);

		while(valorDirPid != pid){

			dirPidAnterior = dirPid;

			dirPid = traerSiguiente(dirPid);

			if(dirPid == NULL){
			/*TODO: crear metodo para agregar la nueva porción de memoria;
			agrega en la dirAnterior una nueva direccóin para agregar el pid en la nueva posiócion */
			}


		}

		return 1;
	}
	return -1; //todo: asignar codigo de error que no se pudo agregar en memoria
}

int traerSiguiente(int dirPid){
	int siguienteDir;
	/*
		Busca en la tabla la direccion que se le pasa por parametro y devuelve la direccion del siguiente eslabon de la cadena
	*/

	return siguienteDir;
}

int traerValor(int pid){
	int valorPid;
	/*
	 * Trae el valor de PID asociado a la direccion que se le pasa
	 */
	return valorPid;
}



/*
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
*/
void dumpMemStructs(void){

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

	if (pid == 0){
		puts("Se muestra info de todos los procesos de Memoria: (no implementado aun)");
	} else {

	int pag, frame;
	char *cont;
	int page_count = pageQuantity(pid);

	printf("PID \t\t PAGINA \t\t CONTENIDO");
	for (pag = 0; pag < page_count; ++pag){
		frame = buscarEnMemoria(pid, pag);
		cont = getMemContent(frame, 0);
		printf("%d \t\t %d \t\t %s\n", pid, pag, cont);
	}

	}
}





