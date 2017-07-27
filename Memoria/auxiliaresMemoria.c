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
extern int size_inv;
extern int pid_free;
extern int free_page;
extern t_log * logTrace;

bool pid_match(int pid, int frame, int off){
	char *dirval = (MEM_FIS + frame * memoria->marco_size + off);
	return (pid == (int) *dirval)? true : false;
}

bool frameLibre(int frame, int off){
	return pid_match(pid_free, frame, off);
}

int pageQuantity(int pid){
	log_trace(logTrace, "Funcion page quantity");
	int off, fr;
	int page_quant = 0;

	for(off = fr = 0; fr * memoria->marco_size + off < size_inv;){
		if (pid_match(pid, fr, off))
			page_quant++;
		nextFrameValue(&fr, &off, sizeof(tEntradaInv));
	}

	return page_quant;
}

int maxPage(int pid){
	log_trace(logTrace, "Funcion max page");

	tEntradaInv *inv;
	int off, fr;
	int max_page = -1;

	for(off = fr = 0; fr * memoria->marco_size + off < size_inv; ){
		if (pid_match(pid, fr, off)){
			inv = (tEntradaInv *) (MEM_FIS + memoria->marco_size * fr + off);
			max_page = MAX(inv->pag, max_page);
		}

		nextFrameValue(&fr, &off, sizeof(tEntradaInv));
	}

	return max_page;
}

int reservarPaginas(int pid, int pageCount){
	log_trace(logTrace,"funcion reservar paginas");
	int fr_apr, fr, off, cic;
	int pag_assign, nr_pag, max_page;
	int init = 1; // distingue si el PID ya tiene paginas

	if ((nr_pag = max_page = maxPage(pid)) == -1){
		log_trace(logTrace, "El PID %d es nuevo en el sistema!", pid);
		nr_pag = max_page = init = 0;
	}

	nr_pag += init;
	pag_assign = cic = 0;
	while (pag_assign != pageCount && cic < memoria->marcos){

		fr_apr = frameHash(pid, nr_pag);
		for(cic = 0; cic < memoria->marcos; cic++){

			gotoFrameInvertida(fr_apr, &fr, &off);
			if (frameLibre(fr, off)){
				memcpy(MEM_FIS + fr * memoria->marco_size +  off, &pid, sizeof (int));
				memcpy(MEM_FIS + fr * memoria->marco_size + (off + sizeof(int)), &nr_pag, sizeof (int));
				pag_assign++; nr_pag++;
				break;
			}
			fr_apr = (fr_apr + 1) % memoria->marcos;
		}
	}

	if (cic == memoria->marcos){ // se recorrio toda la tabla de paginas invertidas
		log_error(logTrace,"no hay mas frames disponibles en memoria para reservar");
		puts("No hay mas frames disponibles en Memoria para reservar.");
		return MEM_TOP_PAGES;
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
	int found;
	for (pag = found = 0; found < lim; ++pag){

		if ((frpid = buscarEnMemoria(pid, pag)) >= 0){
			found++;
			memcpy(MEM_FIS + frpid * sizeof(tEntradaInv), &entry, sizeof entry);
		}
		else{
			log_trace(logTrace,"No se encontro frame para pag %d del pid %d", pag, pid);
			continue;
		}
	}
}

void nextFrameValue(int *fr, int *off, int step_size){
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
