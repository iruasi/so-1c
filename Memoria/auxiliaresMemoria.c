#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#include <tiposRecursos/tiposErrores.h>

#include "apiMemoria.h"
#include "auxiliaresMemoria.h"
#include "memoriaConfigurators.h"


extern char *MEM_FIS;
extern tMemoria *memoria;
extern int marcos_inv;

bool pid_match(int pid, int frame, int off){
	return (pid == (int) (MEM_FIS + frame * memoria->marco_size + off))? true : false ;
}

bool frameLibre(int frame, int off){
	return pid_match(PID_FREE, frame, off);
}

/* En la tabla de paginas invertida, busca que cantidad de paginas tiene asociado
 */
int pageQuantity(int pid){

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
		puts("No hay mas frames disponibles en Memoria para reservar.");
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

/* Dado un numero de frame representativo de la MEM_FIS, modifica fr_inv y off_inv tal que `apunten' al frame
 * correspondiente en la tabla de paginas invertida...
 */
void gotoFrameInvertida(int frame_repr, int *fr_inv, int *off_inv){
	*fr_inv  = frame_repr * sizeof(tEntradaInv) / memoria->marco_size;
	*off_inv = frame_repr * sizeof(tEntradaInv) % memoria->marco_size;
}






