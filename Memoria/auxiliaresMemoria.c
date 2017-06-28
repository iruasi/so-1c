#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#include <tiposRecursos/tiposErrores.h>

#include "apiMemoria.h"
#include "auxiliaresMemoria.h"
#include "memoriaConfigurators.h"

#ifndef PID_FREE
#define PID_FREE -1 // pid disponible
#endif

#ifndef PID_INV
#define PID_INV  -2 // pid tabla invertida
#endif

extern char *MEM_FIS;
extern tMemoria *memoria;
extern int marcos_inv;
extern int pid_free;

bool pid_match(int pid, int frame, int off){
	char *dirval = (MEM_FIS + frame * memoria->marco_size + off);
	return (pid == (int) *dirval)? true : false;
}

bool frameLibre(int frame, int off){
	return pid_match(pid_free, frame, off);
}

int pageQuantity(int pid){

	if (pid < 0){
		fprintf(stderr, "No se puede calcular la cantidad de paginas para el pid %d", pid);
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
	int pag_assign, nr_pag;
	int max_page;
	if ((nr_pag = max_page = pageQuantity(pid)) != 0){
		printf("Fallo conteo de paginas para el pid %d\n", pid);
		return max_page;
	}
	
	// posiciono fr y off al comienzo de las Invertidas que apuntan a frames de memoria utilizables
	gotoFrameInvertida(marcos_inv, &fr, &off);

	pag_assign = 0;
	while (fr < marcos_inv && pag_assign != pageCount){

		if (frameLibre(fr, off)){
			memcpy(MEM_FIS + fr * memoria->marco_size +  off               , &pid     , sizeof (int));
			memcpy(MEM_FIS + fr * memoria->marco_size + (off + sizeof(int)), &nr_pag, sizeof (int));
			pag_assign++;
			nr_pag++;
		}
		nextFrameValue(&fr, &off, sizeof (tEntradaInv));
	}

	if (fr == marcos_inv){
		puts("No hay mas frames disponibles en Memoria para reservar.");
		return MEMORIA_LLENA;
	}

	return max_page; // no sumamos 1 porque contamos desde el 0
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
