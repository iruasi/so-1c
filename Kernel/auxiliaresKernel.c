#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <math.h>

#include <parser/metadata_program.h>

#include "auxiliaresKernel.h"
#include "planificador.h"

#include <funcionesCompartidas/funcionesCompartidas.h>
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

	nuevoPCB->indiceDeStack = list_create();

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
	nuevoPCB->estado_proc = 0;
	nuevoPCB->contextoActual = 0; // todo: verificar que esta inicializacion sea coherente
	nuevoPCB->cantidad_instrucciones = meta->instrucciones_size;
	nuevoPCB->indiceDeCodigo->start  = meta->instrucciones_serializado->start;
	nuevoPCB->indiceDeCodigo->offset = meta->instrucciones_serializado->offset;

	nuevoPCB->indiceDeEtiquetas = meta->etiquetas;

	nuevoPCB->exitCode = 0;

	//almacenar(nuevoPCB->id, meta);

	return nuevoPCB;
}
