#include <stdbool.h>
#include <math.h>
#include <string.h>

#include "../Compartidas/tiposErrores.h"

#include "apiMemoria.h"
#include "auxiliaresMemoria.h"
#include "memoriaConfigurators.h"


extern char *MEM_FIS;
extern tMemoria *memoria;
extern int size_inv_total;
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
		nextFrameValue(&fr, &off, sizeof(tEntradaInv));
		if (pid_match(pid, fr, off))
			page_quant++;
	}

	return page_quant;
}

int reservarCodYStackInv(int pid, int pageCount){

	int fr, off, fr_start, off_start;
	int contig_pages = 0; // paginas contiguas
	int max_page = pageQuantity(pid);

	off = fr = 0;
	while (fr < marcos_inv && contig_pages != pageCount){

		fr_start = fr; off_start = off;
		for (contig_pages = 0; frameLibre(fr, off) && fr < marcos_inv && contig_pages < pageCount; contig_pages++){
			nextFrameValue(&fr_start, &off_start, sizeof (tEntradaInv));
		}
		nextFrameValue(&fr_start, &off_start, sizeof (tEntradaInv));
	}

	for (contig_pages = 0; contig_pages < pageCount; contig_pages++){
		memcpy(MEM_FIS + fr_start * memoria->marco_size + off_start, &pid, sizeof pid);
		nextFrameValue(&fr_start, &off_start, sizeof (int));
		memcpy(MEM_FIS + fr_start * memoria->marco_size + off_start, &max_page, sizeof max_page);
		nextFrameValue(&fr_start, &off_start, sizeof (int));
	}

	return contig_pages;
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

/*
 *  Esta funcion tiene que ir 'pasando los frames' hasta llegar al necesario,
 *  y despues ir corriendo el offset hasta que apunta al frame_representativo
 */
	if ((frame_repr * memoria->marco_size) % sizeof(tEntradaInv) != 0){
//todo: esto lo rompo a proposito para trabajarlo ahora...
	}
}
