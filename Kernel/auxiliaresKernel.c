#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <math.h>

#include "auxiliaresKernel.h"

#include "planificador.h"

#include "../Compartidas/funcionesPaquetes.h"
#include "../Compartidas/tiposErrores.h"
#include "../Compartidas/tiposPaquetes.h"
#include "../Compartidas/pcb.h"

uint32_t globalPID = 0;

int procesarYCrearPrograma(tPackHeader *head, int fd_sender, int fd_mem, uint8_t stack_size, int frame_size){

	int stat;
	tProceso proc = KER;
	tMensaje msj  = INI_PROG;

	int packageSize; // aca guardamos el tamanio total del paquete serializado
	int src_size;


	void *pack_src_serial = serializeSrcCodeFromRecv(fd_sender, *head, &packageSize);
	if (pack_src_serial == NULL){
		puts("Fallo al recibir y serializar codigo fuente");
		return FALLO_GRAL;
	}

	src_size = packageSize - (HEAD_SIZE + sizeof(unsigned long));
	tPCB *new_pcb = nuevoPCB((int) ceil((float) src_size + stack_size / frame_size));

	encolarPrograma(new_pcb, fd_sender);// TODO: le damos el PCB al planificador a esta altura ya?


	// pisamos el header del paquete serializado
	memcpy(pack_src_serial              , &proc, sizeof proc);
	memcpy(pack_src_serial + sizeof proc, &msj, sizeof msj);

	if ((stat = send(fd_mem, pack_src_serial, packageSize, 0)) < 0){
		perror("Error en el envio de codigo fuente. error");
		return FALLO_SEND;
	}

	free(pack_src_serial);
	return 0;
}

tPCB *nuevoPCB(int cant_pags){

	tPCB *nuevoPCB = malloc(sizeof *nuevoPCB);

	nuevoPCB->id = globalPID;
	globalPID++;
	nuevoPCB->pc = 0;
	nuevoPCB->paginasDeCodigo   = cant_pags;
	nuevoPCB->indiceDeCodigo    = NULL;
	nuevoPCB->indiceDeEtiquetas = NULL;
	nuevoPCB->indiceDeStack     = NULL;
	nuevoPCB->exitCode = 0;

	return nuevoPCB;

}

