#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <commons/log.h>
#include <tiposRecursos/tiposErrores.h>

#include "apiMemoria.h"
#include "manejadoresMem.h"
#include "auxiliaresMemoria.h"
#include "memoriaConfigurators.h"

#ifndef PID_FREE
#define PID_FREE -1 // pid disponible
#endif

#ifndef PID_INV
#define PID_INV  -2 // pid tabla invertida
#endif

extern tCacheEntrada *CACHE_lines;
extern char *MEM_FIS;
extern tMemoria *memoria;
extern int marcos_inv;
extern int pid_free;
extern int free_page;
extern t_log * logTrace;

bool pid_match(int pid, int frame, int off){
	log_trace(logTrace,"funcion pidmatch");
	char *dirval = (MEM_FIS + frame * memoria->marco_size + off);
	return (pid == (int) *dirval)? true : false;
}

bool frameLibre(int frame, int off){
	log_trace(logTrace,"funcion frame libre");
	return pid_match(pid_free, frame, off);
}

int pageQuantity(int pid){
	log_trace(logTrace,"funcion page quantity");
	int off, fr;
	int page_quant = 0;

	for(off = fr = 0; fr < marcos_inv;){
		if (pid_match(pid, fr, off))
			page_quant++;
		nextFrameValue(&fr, &off, sizeof(tEntradaInv));
	}

	return page_quant;
}

int reservarPaginas(int pid, int pageCount){
	log_trace(logTrace,"funcion reservar paginas");
	int fr_apr, fr, off, cic;
	int pag_assign, nr_pag, max_page;

	if ((nr_pag = max_page = pageQuantity(pid)) < 0){
		log_error(logTrace,"fallo conteo de pags para el pid %d",pid);
		printf("Fallo conteo de paginas para el pid %d\n", pid);
		return max_page;
	}
	
	pag_assign = cic = 0;
	while (pag_assign != pageCount && cic < memoria->marcos){

		fr_apr = frameHash(pid, nr_pag);
		for(cic = 0; cic < memoria->marcos; cic++){

			gotoFrameInvertida(fr_apr, &fr, &off);
			if (frameLibre(fr, off)){
				memcpy(MEM_FIS + fr * memoria->marco_size +  off               , &pid    , sizeof (int));
				memcpy(MEM_FIS + fr * memoria->marco_size + (off + sizeof(int)), &nr_pag , sizeof (int));
				pag_assign++; nr_pag++;
				break;
			}
			fr_apr = (fr_apr + 1) % memoria->marcos;
		}
	}

	if (cic == memoria->marcos){ // se recorrio toda la tabla de paginas invertidas
		log_error(logTrace,"no hay mas frames disponibles en memoria para reservar");
		puts("No hay mas frames disponibles en Memoria para reservar.");
		return MEMORIA_LLENA;
	}

	return max_page; // no sumamos 1 porque contamos desde el 0
}

void limpiarDeCache(int pid){
	log_trace(logTrace,"funcion limpiar cache");
	int i;
	for (i = 0; i < memoria->entradas_cache; ++i){
		if (CACHE_lines[i].pid == pid){
			CACHE_lines[i].pid  = pid_free;
			CACHE_lines[i].page = free_page;
		}
	}
}

void limpiarDeInvertidas(int pid){
	log_trace(logTrace,"funcion limpiar de invertidas");
	int pag, frpid;
	tEntradaInv entry = {.pid = pid_free, .pag = free_page};

	int lim = pageQuantity(pid);
	for (pag = 0; pag < lim; ++pag){

		if ((frpid = buscarEnMemoria(pid, pag)) >= 0)
			memcpy(MEM_FIS + frpid * sizeof(tEntradaInv), &entry, sizeof entry);
		else
			log_trace(logTrace,"no se encontro frame para la pag %d del pid %d. posible fragmentacion",pag,pid);
			printf("No se encontro frame para la pagina %d del pid %d. Posible fragmentacion\n", pag, pid);
	}
}

void nextFrameValue(int *fr, int *off, int step_size){
	log_trace(logTrace,"funcion next frame value");
	if(*off + step_size >= memoria->marco_size){
		(*fr)++; *off = 0;
	} else {
		(*off) += step_size;
	}
}

void gotoFrameInvertida(int frame_repr, int *fr_inv, int *off_inv){
	*fr_inv  = frame_repr * sizeof(tEntradaInv) / memoria->marco_size;
	*off_inv = frame_repr * sizeof(tEntradaInv) % memoria->marco_size;
}
