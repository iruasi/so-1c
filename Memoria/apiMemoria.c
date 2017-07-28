#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <semaphore.h>


#include <tiposRecursos/tiposErrores.h>
#include <tiposRecursos/tiposPaquetes.h>

#include <commons/log.h>

#include "manejadoresMem.h"
#include "manejadoresCache.h"
#include "apiMemoria.h"
#include "structsMem.h"
#include "auxiliaresMemoria.h"

float retardo_mem; // latencia de acceso a Memoria Fisica
extern tMemoria *memoria;
extern tCacheEntrada *CACHE_lines;
extern int pid_free, free_page;
extern t_log * logTrace;
// OPERACIONES DE LA MEMORIA


void retardo(int ms){
	retardo_mem = ms / 1000.0;
	printf("Se cambio la latencia de acceso a Memoria a %f segundos\n", retardo_mem);
	log_trace(logTrace,"Se cambio la latencia de acceso a memoria");
}

void flush(void){
	log_trace(logTrace,"FLUSH");
	int i;
	for (i = 0; i < memoria->entradas_cache; ++i){
		(CACHE_lines +i)->pid  = pid_free;
		(CACHE_lines +i)->page = free_page;
	}
}

void dump(int pid){

	puts("COMIENZO DE DUMP");
	log_trace(logTrace,"DUMP [PID %d]",pid);
	dumpCache();
	dumpMemStructs();
	dumpMemContent(pid);
	log_trace(logTrace,"FIN DUMP");
	puts("FIN DEL DUMP");
}


void size(int pid){
	log_trace(logTrace,"funcion size[PID %d]",pid);
	int mem_ocup, mem_free = 0;

	if (pid < 0){
		mem_free = pageQuantity(pid_free);
		mem_ocup = memoria->marcos - mem_free;

		printf("Cantidad de frames en Memoria: %d\n", memoria->marcos);
		printf("Cantidad de frames ocupados:   %d\n", mem_ocup);
		printf("Cantidad de frames libres:     %d\n", mem_free);

	} else
		printf("Size del proceso %d: %d Bytes\n", pid, pageQuantity(pid) * memoria->marco_size);
}


// API DE LA MEMORIA

int inicializarPrograma(int pid, int page_quant){
	log_trace(logTrace,"Se reservan %d paginas[PID %d]", page_quant,pid);
	int reservadas;

	if ((reservadas = reservarPaginas(pid, page_quant)) < 0){
		log_error(logTrace, "Fallo reserva: No hay frames libres en Memoria[PID %d]",pid);
		return reservadas;
	}
	log_trace(logTrace,"Se reservaron %d paginas a partir de la %d", page_quant, reservadas);
	return 0;
}

int almacenarBytes(int pid, int page, int offset, int size, char *buffer){

	log_trace(logTrace, "Se almacenan para el PID %d: %d bytes en la pag %d", pid, size, page);
	int frame;
	if ((frame = buscarEnMemoria(pid, page)) < 0){
		log_error(logTrace,"Fallo buscar en Memoria el pid %d y pagina %d", pid, page);
		return frame;
	}

	memcpy(MEM_FIS + frame * memoria->marco_size + offset, buffer, size);
	actualizarCache(pid, page, frame);

	return 0;
}

char *solicitarBytes(int pid, int page, int offset, int size){

	log_trace(logTrace,"Se solicitan para el PID %d: %d bytes de la pagina %d", pid, size, page);
	char *buffer;
	if ((buffer = leerBytes(pid, page, offset, size)) == NULL){
		log_error(logTrace,"no se pudieron leer los bytes de la pag");
		puts("No se pudieron leer los bytes de la pagina");
		return NULL;
	}
	return buffer;
}

int asignarPaginas(int pid, int page_count){
	log_trace(logTrace,"funcion asignar paginas cant pags %d [PID %d]",page_count,pid);
	int new_page, stat;
	tHeapMeta hmd = {.size = memoria->marco_size - 2*SIZEOF_HMD, .isFree = true};

	if((new_page = reservarPaginas(pid, page_count)) < 0){
		log_error(logTrace,"no se pudieron reservar pags para el proceso [PID %d]",pid);
		printf("No se pudieron reservar paginas para el proceso. error: %d\n", new_page);
		return new_page;
	}

	// escribimos el primer HeapMetaData
	if ((stat = almacenarBytes(pid, new_page, 0, SIZEOF_HMD, (char *) &hmd)) != 0)
		return stat;

	//printf("Se reservaron correctamente %d paginas\n", page_count);
	log_trace(logTrace,"se reservaron correctamente %d paginas [PID %d]",page_count,pid);
	return new_page;
}

/* Llamado por Kernel, libera una pagina de HEAP.
 * Retorna MEM_EXCEPTION si no puede liberar la pagina porque no existe o
 * porque simplemente no puede hacerse
 */
int liberarPagina(int pid, int page){
	//printf("Se libera la pagina %d del PID %d\n", page, pid);
	log_trace(logTrace,"se libera la pag %d del pid %d",page,pid);
	int frame;
	tEntradaInv entry = {.pid = pid_free, .pag = free_page};

	if ((frame = buscarEnMemoria(pid, page)) >= 0){
		memcpy(MEM_FIS + frame * memoria->marco_size, &entry, sizeof entry);
		return 0;
	}
	log_error(logTrace,"no se encontro el frame para la pag %d del pid %d",page,pid);
	//printf("No se encontro el frame para la pagina %d del pid %d. Fallo liberacion\n", page, pid);
	return MEM_EXCEPTION;
}


void finalizarPrograma(int pid){
	//printf("Se procede a finalizar el pid %d\n", pid);
	log_trace(logTrace,"se procede a finalizar el pid %d",pid);
	dump(pid);

	limpiarDeCache(pid);
	limpiarDeInvertidas(pid);
}

