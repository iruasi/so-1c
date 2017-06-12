#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#include <tiposRecursos/tiposErrores.h>

#include <commons/log.h>

#include "apiMemoria.h"
#include "auxiliaresMemoria.h"
#include "memoriaConfigurators.h"

#define PID_FREE  0 // pid disponible
#define PID_INV  -1 // pid tabla invertida

extern t_log * logger;
extern char *MEM_FIS;
extern tMemoria *memoria;
extern int marcos_inv;

bool pid_match(int pid, int frame, int off){
	return (pid == (int) (MEM_FIS + frame * memoria->marco_size + off))? true : false ;
}

bool frameLibre(int frame, int off){
	return pid_match(PID_FREE, frame, off);
}

int pageQuantity(int pid){

	if (pid < 0){
		log_error(logger,"No se puede calcualr la cantidad de paginas para el pid %d",pid);

		return PID_INVALIDO;
	}

	int i, off, fr;
	int page_quant = 0;

	for(i = off = fr = 0; i < marcos_inv; ++i){
		if (pid_match(pid, fr, off))
			page_quant++;
		nextFrameValue(&fr, &off, sizeof(tEntradaInv));
	}

	return page_quant;
}

int reservarPaginas(int pid, int pageCount){

	int fr, off;
	int pag_assign;
	int max_page = pageQuantity(pid);

	gotoFrameInvertida(marcos_inv, &fr, &off);
	pag_assign = 0;
	while (fr < marcos_inv && pag_assign != pageCount){

		if (frameLibre(fr, off)){
			memcpy(MEM_FIS + fr * memoria->marco_size + off, &pid, sizeof pid);
			nextFrameValue(&fr, &off, sizeof (int));
			memcpy(MEM_FIS + fr * memoria->marco_size + off, &max_page, sizeof max_page);
			pag_assign++;
		}
		nextFrameValue(&fr, &off, sizeof (int));

	}

	if (fr == marcos_inv){
		log_error(logger,"No hay mas frames disponibles en Memoria para reservar");

		return MEMORIA_LLENA;
	}

	return 0;
}

void nextFrameValue(int *fr, int *off, int step_size){
	if(*off + step_size > memoria->marco_size){
		(*fr)++; *off = 0;
	} else {
		(*off) += step_size;
	}
}

void gotoFrameInvertida(int frame_repr, int *fr_inv, int *off_inv){
	*fr_inv  = frame_repr * sizeof(tEntradaInv) / memoria->marco_size;
	*off_inv = frame_repr * sizeof(tEntradaInv) % memoria->marco_size;
}
