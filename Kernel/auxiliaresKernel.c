#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <math.h>

#include <parser/metadata_program.h>

#include "auxiliaresKernel.h"
#include "planificador.h"

#include <funcionesPaquetes/funcionesPaquetes.h>
#include <tiposRecursos/tiposErrores.h>
#include <tiposRecursos/tiposPaquetes.h>
#include <tiposRecursos/misc/pcb.h>

#ifndef HEAD_SIZE
#define HEAD_SIZE 8
#endif

extern t_log* logger;

uint32_t globalPID;



int passSrcCodeFromRecv(tPackHeader *head, int fd_sender, int fd_mem, int *src_size){

	int stat;
	tProceso proc = KER;
	tMensaje msj  = SRC_CODE;

	int packageSize; // aca guardamos el tamanio total del paquete serializado

	puts("Entremos a serializeSrcCodeFromRecv");
	void *pack_src_serial = serializeSrcCodeFromRecv(fd_sender, *head, &packageSize);

	if (pack_src_serial == NULL){
		puts("Fallo al recibir y serializar codigo fuente");
		return FALLO_GRAL;
	}

	*src_size = packageSize - (HEAD_SIZE + sizeof(unsigned long));

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


// todo: persisitir
tPCB *nuevoPCB(tPackSrcCode *src_code, int cant_pags){

	t_metadata_program *meta = metadata_desde_literal(src_code->sourceCode);

	tPCB *nuevoPCB = malloc(sizeof *nuevoPCB);
	nuevoPCB->indiceDeCodigo = malloc(sizeof nuevoPCB->indiceDeCodigo);
	nuevoPCB->indiceDeStack = malloc(sizeof nuevoPCB->indiceDeStack + sizeof nuevoPCB->indiceDeStack->args
			+ sizeof nuevoPCB->indiceDeStack->retVar + sizeof nuevoPCB->indiceDeStack->vars);
	nuevoPCB->indiceDeStack->args   = malloc(sizeof nuevoPCB->indiceDeStack->args);
	nuevoPCB->indiceDeStack->retVar = malloc(sizeof nuevoPCB->indiceDeStack->retVar);
	nuevoPCB->indiceDeStack->vars   = malloc(sizeof nuevoPCB->indiceDeStack->vars);

	nuevoPCB->indiceDeEtiquetas = malloc(nuevoPCB->etiquetaSize);

	nuevoPCB->id = globalPID;
	globalPID++;
	nuevoPCB->pc = 0;
	nuevoPCB->paginasDeCodigo = cant_pags;

	nuevoPCB->cantidad_instrucciones = meta->instrucciones_size;
	nuevoPCB->indiceDeCodigo->start  = meta->instrucciones_serializado->start;
	nuevoPCB->indiceDeCodigo->offset = meta->instrucciones_serializado->offset;

	nuevoPCB->indiceDeEtiquetas = meta->etiquetas;
	//nuevoPCB->indiceDeStack = NULL;
	nuevoPCB->exitCode = 0;


	//almacenar(nuevoPCB->id, meta);

	return nuevoPCB;

}

// TODO: arreglar esto, revisar bien las asignaciones de tamanios y demas...
char *serializePCB(tPCB *pcb, tPackHeader head){

	int off = 0;
	char *pcb_serial;
	int serial_size;
	int i;

	if ((pcb_serial = malloc(HEAD_SIZE + sizeof *pcb)) == NULL){
		fprintf(stderr, "No se pudo mallocar espacio para pcb serializado\n");
		return NULL;
	}
	serial_size = HEAD_SIZE + sizeof *pcb;

	memcpy(pcb_serial + off, &pcb->id, sizeof (int));
	off += sizeof (int);
	memcpy(pcb_serial + off, &pcb->pc, sizeof (int));
	off += sizeof (int);
	memcpy(pcb_serial + off, &pcb->paginasDeCodigo, sizeof (int));
	off += sizeof (int);
	memcpy(pcb_serial + off, &pcb->cantidad_instrucciones, sizeof (int));
	off += sizeof (int);
	memcpy(pcb_serial + off, &pcb->exitCode, sizeof (int));
	off += sizeof (int);

	// alloc indice de codigo

	serial_size += sizeof (t_intructions);
	realloc (pcb_serial, serial_size);

	memcpy(pcb_serial + off, &pcb->indiceDeCodigo->start, sizeof pcb->indiceDeCodigo->start);
	off += sizeof pcb->indiceDeCodigo->start;
	memcpy(pcb_serial + off, &pcb->indiceDeCodigo->offset, sizeof pcb->indiceDeCodigo->offset);
	off += sizeof pcb->indiceDeCodigo->offset;

	// alloc indice de stack
	int serial_save = serial_size;
	serial_size += sizeof pcb->indiceDeStack->args + sizeof pcb->indiceDeStack->retPos
			+ sizeof pcb->indiceDeStack->retVar + sizeof pcb->indiceDeStack->vars;
	char *memoria = malloc(serial_size);
	memcpy(memoria, pcb_serial, serial_save);
	//realloc(pcb_serial, serial_size);

	int cant = sizeof *(pcb->indiceDeStack->args) / sizeof (int);
	for (i = 0; i < cant; ++i){
		memcpy(memoria + off, &(pcb->indiceDeStack->args)[i], sizeof (int));
		off += sizeof (int);
	}

	cant = sizeof pcb->indiceDeStack->vars / sizeof (int);
	for (i = 0; i < cant; ++i){
		memcpy(memoria + off, &(pcb->indiceDeStack->vars)[i], sizeof (int));
		off += sizeof (int);
	}

	memcpy(memoria + off, &pcb->indiceDeStack->retPos, sizeof (int));
	off += sizeof (int);

	cant = sizeof pcb->indiceDeStack->retVar / sizeof (int);
	for (i = 0; i < cant; ++i){
		memcpy(memoria + off, &(pcb->indiceDeStack->retVar)[i], sizeof (int));
		off += sizeof (int);
	}

	// realloc indice de etiquetas ya se alloco etiquetas_size
	serial_size += pcb->etiquetaSize;
	realloc(memoria, serial_size);

	memcpy(memoria + off, &pcb->indiceDeEtiquetas, pcb->etiquetaSize);
	off += sizeof pcb->etiquetaSize;

	return memoria;



}





