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

	bool hayEtiquetas = (meta->etiquetas_size > 0)? true : false;

	tPCB *nuevoPCB = malloc(sizeof *nuevoPCB);
	nuevoPCB->indiceDeCodigo = malloc(sizeof nuevoPCB->indiceDeCodigo);
	nuevoPCB->indiceDeStack = malloc(sizeof nuevoPCB->indiceDeStack + sizeof nuevoPCB->indiceDeStack->args
			+ sizeof nuevoPCB->indiceDeStack->retVar + sizeof nuevoPCB->indiceDeStack->vars);
	nuevoPCB->indiceDeStack->args   = malloc(sizeof nuevoPCB->indiceDeStack->args);
	nuevoPCB->indiceDeStack->retVar = malloc(sizeof nuevoPCB->indiceDeStack->retVar);
	nuevoPCB->indiceDeStack->vars   = malloc(sizeof nuevoPCB->indiceDeStack->vars);

	nuevoPCB->etiquetaSize = 0;
	if (hayEtiquetas){
		nuevoPCB->etiquetaSize = meta->etiquetas_size;
		nuevoPCB->indiceDeEtiquetas = malloc(nuevoPCB->etiquetaSize);
		memcpy(nuevoPCB->indiceDeEtiquetas, meta->etiquetas, nuevoPCB->etiquetaSize);
	}

	nuevoPCB->id = globalPID;
	globalPID++;
	nuevoPCB->pc = 0;
	nuevoPCB->paginasDeCodigo = cant_pags;

	nuevoPCB->cantidad_instrucciones = meta->instrucciones_size;
	nuevoPCB->indiceDeCodigo->start  = meta->instrucciones_serializado->start;
	nuevoPCB->indiceDeCodigo->offset = meta->instrucciones_serializado->offset;

	nuevoPCB->indiceDeEtiquetas = meta->etiquetas;
	nuevoPCB->indiceDeStack->args->pag = -1;
	nuevoPCB->indiceDeStack->args->offset = -1;
	nuevoPCB->indiceDeStack->args->size = -1;

	nuevoPCB->indiceDeStack->vars->pid = -1;
	nuevoPCB->indiceDeStack->vars->pos.pag = -1;
	nuevoPCB->indiceDeStack->vars->pos.offset = -1;
	nuevoPCB->indiceDeStack->vars->pos.size = -1;

	nuevoPCB->indiceDeStack->retPos = -1;

	nuevoPCB->indiceDeStack->retVar->pag = -1;
	nuevoPCB->indiceDeStack->retVar->offset = -1;
	nuevoPCB->indiceDeStack->retVar->size = -1;

	nuevoPCB->exitCode = 0;

	//almacenar(nuevoPCB->id, meta);

	return nuevoPCB;
}

// TODO: arreglar esto, revisar bien las asignaciones de tamanios y demas...
char *serializePCB(tPCB *pcb, tPackHeader head){

	int off = 0;
	char *pcb_serial;
	bool hayEtiquetas = (pcb->etiquetaSize > 0)? true : false;

	size_t ctesInt_size         = 6 * sizeof (int);
	size_t indiceCod_size       = sizeof (t_puntero_instruccion) + sizeof (t_size);
	size_t indiceStack_size     = 2 * sizeof (posicionMemoria) + sizeof (posicionMemoriaPid) + sizeof (int);
	size_t indiceEtiquetas_size = (size_t) pcb->etiquetaSize;

	if ((pcb_serial = malloc(HEAD_SIZE + ctesInt_size + indiceCod_size + indiceStack_size + indiceEtiquetas_size)) == NULL){
		fprintf(stderr, "No se pudo mallocar espacio para pcb serializado\n");
		return NULL;
	}

	memcpy(pcb_serial + off, &pcb->id, sizeof (int));
	off += sizeof (int);
	memcpy(pcb_serial + off, &pcb->pc, sizeof (int));
	off += sizeof (int);
	memcpy(pcb_serial + off, &pcb->etiquetaSize, sizeof (int));
	off += sizeof (int);
	memcpy(pcb_serial + off, &pcb->paginasDeCodigo, sizeof (int));
	off += sizeof (int);
	memcpy(pcb_serial + off, &pcb->cantidad_instrucciones, sizeof (int));
	off += sizeof (int);
	memcpy(pcb_serial + off, &pcb->exitCode, sizeof (int));
	off += sizeof (int);

	// serializamos indice de codigo
	memcpy(pcb_serial + off, &pcb->indiceDeCodigo->start, sizeof pcb->indiceDeCodigo->start);
	off += sizeof pcb->indiceDeCodigo->start;
	memcpy(pcb_serial + off, &pcb->indiceDeCodigo->offset, sizeof pcb->indiceDeCodigo->offset);
	off += sizeof pcb->indiceDeCodigo->offset;

	// serializamos indice de stack
	// args:
	memcpy(pcb_serial + off, &pcb->indiceDeStack->args->pag, sizeof pcb->indiceDeStack->args->pag);
	off += sizeof pcb->indiceDeStack->args->pag;
	memcpy(pcb_serial + off, &pcb->indiceDeStack->args->offset, sizeof pcb->indiceDeStack->args->offset);
	off += sizeof pcb->indiceDeStack->args->offset;
	memcpy(pcb_serial + off, &pcb->indiceDeStack->args->size, sizeof pcb->indiceDeStack->args->size);
	off += sizeof pcb->indiceDeStack->args->size;

	// vars:
	memcpy(pcb_serial + off, &pcb->indiceDeStack->vars->pid, sizeof pcb->indiceDeStack->vars->pid);
	off += sizeof pcb->indiceDeStack->vars->pid;
	memcpy(pcb_serial + off, &pcb->indiceDeStack->vars->pos.pag, sizeof pcb->indiceDeStack->vars->pos.pag);
	off += sizeof pcb->indiceDeStack->vars->pos.pag;
	memcpy(pcb_serial + off, &pcb->indiceDeStack->vars->pos.offset, sizeof pcb->indiceDeStack->vars->pos.offset);
	off += sizeof pcb->indiceDeStack->vars->pos.offset;
	memcpy(pcb_serial + off, &pcb->indiceDeStack->vars->pos.size, sizeof pcb->indiceDeStack->vars->pos.size);
	off += sizeof pcb->indiceDeStack->vars->pos.size;

	// retPos:
	memcpy(pcb_serial + off, &pcb->indiceDeStack->retPos, sizeof pcb->indiceDeStack->retPos);
	off += sizeof pcb->indiceDeStack->retPos;

	// retVar
	memcpy(pcb_serial + off, &pcb->indiceDeStack->retVar->pag, sizeof pcb->indiceDeStack->retVar->pag);
	off += sizeof pcb->indiceDeStack->retVar->pag;
	memcpy(pcb_serial + off, &pcb->indiceDeStack->retVar->offset, sizeof pcb->indiceDeStack->retVar->offset);
	off += sizeof pcb->indiceDeStack->retVar->offset;
	memcpy(pcb_serial + off, &pcb->indiceDeStack->retVar->size, sizeof pcb->indiceDeStack->retVar->size);
	off += sizeof pcb->indiceDeStack->retVar->size;

	// serializamos indice de etiquetas
	if (hayEtiquetas){
		memcpy(pcb_serial + off, pcb->indiceDeEtiquetas, pcb->etiquetaSize);
		off += sizeof pcb->etiquetaSize;
	}

	return pcb_serial;
}





