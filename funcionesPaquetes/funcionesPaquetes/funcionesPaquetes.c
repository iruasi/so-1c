#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>

#include <tiposRecursos/tiposErrores.h>
#include <tiposRecursos/tiposPaquetes.h>
#include <tiposRecursos/misc/pcb.h>
#include "funcionesPaquetes.h"

/*
 * funcionesPaquetes es el modulo que retiene el gruso de las funciones
 * que se utilizaran para operar con serializacion y deserializacion de
 * estructuras, el envio de informaciones particulares en los diferentes
 * handshakes entre procesos, y funciones respecto de paquetes en general
 */


/****** Definiciones de Handshakes ******/

int contestarMemoriaKernel(int marco_size, int marcos, int sock_ker){

	int stat;

	tHShakeMemAKer *h_shake = malloc(sizeof *h_shake);
	h_shake->head.tipo_de_proceso = MEM;
	h_shake->head.tipo_de_mensaje = MEMINFO;
	h_shake->marco_size = marco_size;
	h_shake->marcos = marcos;

	if((stat = send(sock_ker, h_shake, sizeof *h_shake, 0)) == -1)
		perror("Error de envio informacion Memoria a Kernel. error");

	return stat;
}

int recibirInfoMem(int sock_mem, int *frames, int *frame_size){

	int stat;

	if ((stat = recv(sock_mem, frames, sizeof frames, 0)) == -1){
		perror("Error de recepcion de frames desde la Memoria. error");
		return FALLO_GRAL;
	}

	if ((stat = recv(sock_mem, frame_size, sizeof frame_size, 0)) == -1){
		perror("Error de recepcion de frames desde la Memoria. error");
		return FALLO_GRAL;
	}

	return stat;
}


/****** Definiciones de [De]Serializaciones ******/


char *serializePCB(tPCB *pcb, tPackHeader head, int *pack_size){

	int off = 0;
	char *pcb_serial;
	bool hayEtiquetas = (pcb->etiquetaSize > 0)? true : false;

	size_t ctesInt_size         = 6 * sizeof (int);
	size_t indiceCod_size       = sizeof (t_puntero_instruccion) + sizeof (t_size);
	size_t indiceStack_size     = 2 * sizeof (posicionMemoria) + sizeof (posicionMemoriaPid) + sizeof (int);
	size_t indiceEtiquetas_size = (size_t) pcb->etiquetaSize;

	if ((pcb_serial = malloc(HEAD_SIZE + sizeof(int) + ctesInt_size + indiceCod_size + indiceStack_size + indiceEtiquetas_size)) == NULL){
		fprintf(stderr, "No se pudo mallocar espacio para pcb serializado\n");
		return NULL;
	}

	memcpy(pcb_serial + off, &head, HEAD_SIZE);
	off += HEAD_SIZE;

	// incremento para dar lugar al size_total al final del serializado
	off += sizeof(int);

	memcpy(pcb_serial + off, &pcb->id, sizeof (int));
	off += sizeof (int);
	memcpy(pcb_serial + off, &pcb->pc, sizeof (int));
	off += sizeof (int);
	memcpy(pcb_serial + off, &pcb->paginasDeCodigo, sizeof (int));
	off += sizeof (int);
	memcpy(pcb_serial + off, &pcb->etiquetaSize, sizeof (int));
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

	memcpy(pcb_serial + HEAD_SIZE, &off, sizeof(int));
	*pack_size = off;

	return pcb_serial;
}

tPCB *deserializarPCB(char *pcb_serial){

	int offset = 0;
	size_t ctesInt_size         = 6 * sizeof (int);
	size_t indiceCod_size       = sizeof (t_puntero_instruccion) + sizeof (t_size);
	size_t indiceStack_size     = 2 * sizeof (posicionMemoria) + sizeof (posicionMemoriaPid) + sizeof (int);

	tPCB *pcb;
	if ((pcb = malloc(ctesInt_size)) == NULL){
		fprintf(stderr, "Fallo malloc\n");
		return NULL;
	}

	if ((pcb->indiceDeCodigo = malloc(indiceCod_size)) == NULL){
		fprintf(stderr, "Fallo malloc\n");
		return NULL;
	}

	if ((pcb->indiceDeStack = malloc(indiceStack_size)) == NULL){
		fprintf(stderr, "Fallo malloc\n");
		return NULL;
	}

	if ((pcb->indiceDeStack->args = malloc(sizeof(posicionMemoria))) == NULL){
		fprintf(stderr, "Fallo malloc\n");
		return NULL;
	}
	if ((pcb->indiceDeStack->vars = malloc(sizeof(int) + sizeof(posicionMemoria))) == NULL){
		fprintf(stderr, "Fallo malloc\n");
		return NULL;
	}
	if ((pcb->indiceDeStack->retVar = malloc(sizeof(posicionMemoria))) == NULL){
		fprintf(stderr, "Fallo malloc\n");
		return NULL;
	}

	memcpy(&pcb->id, pcb_serial + offset, sizeof(int));
	offset += sizeof(int);
	memcpy(&pcb->pc, pcb_serial + offset, sizeof(int));
	offset += sizeof(int);
	memcpy(&pcb->paginasDeCodigo, pcb_serial + offset, sizeof(int));
	offset += sizeof(int);
	memcpy(&pcb->etiquetaSize, pcb_serial + offset, sizeof(int));
	offset += sizeof(int);
	memcpy(&pcb->cantidad_instrucciones, pcb_serial + offset, sizeof(int));
	offset += sizeof(int);
	memcpy(&pcb->exitCode, pcb_serial + offset, sizeof(int));
	offset += sizeof(int);

	memcpy(&pcb->indiceDeCodigo->start, pcb_serial + offset, sizeof (pcb->indiceDeCodigo->start));
	offset += sizeof (pcb->indiceDeCodigo->start);
	memcpy(&pcb->indiceDeCodigo->offset, pcb_serial + offset, sizeof (pcb->indiceDeCodigo->offset));
	offset += sizeof (pcb->indiceDeCodigo->offset);

	memcpy(&pcb->indiceDeStack->args->pag, pcb_serial + offset, sizeof (pcb->indiceDeStack->args->pag));
	offset += sizeof (pcb->indiceDeStack->args->pag);
	memcpy(&pcb->indiceDeStack->args->offset, pcb_serial + offset, sizeof (pcb->indiceDeStack->args->offset));
	offset += sizeof (pcb->indiceDeStack->args->offset);
	memcpy(&pcb->indiceDeStack->args->size, pcb_serial + offset, sizeof (pcb->indiceDeStack->args->size));
	offset += sizeof (pcb->indiceDeStack->args->size);

	memcpy(&pcb->indiceDeStack->vars->pid, pcb_serial + offset, sizeof (pcb->indiceDeStack->vars->pid));
	offset += sizeof (pcb->indiceDeStack->vars->pid);
	memcpy(&pcb->indiceDeStack->vars->pos.pag, pcb_serial + offset, sizeof (pcb->indiceDeStack->vars->pos.pag));
	offset += sizeof (pcb->indiceDeStack->vars->pos.pag);
	memcpy(&pcb->indiceDeStack->vars->pos.offset, pcb_serial + offset, sizeof (pcb->indiceDeStack->vars->pos.offset));
	offset += sizeof (pcb->indiceDeStack->vars->pos.offset);
	memcpy(&pcb->indiceDeStack->vars->pos.size, pcb_serial + offset, sizeof (pcb->indiceDeStack->vars->pos.size));
	offset += sizeof (pcb->indiceDeStack->vars->pos.size);

	memcpy(&pcb->indiceDeStack->retPos, pcb_serial + offset, sizeof (pcb->indiceDeStack->retPos));
	offset += sizeof (pcb->indiceDeStack->retPos);

	memcpy(&pcb->indiceDeStack->retVar->pag, pcb_serial + offset, sizeof (pcb->indiceDeStack->retVar->pag));
	offset += sizeof (pcb->indiceDeStack->retVar->pag);
	memcpy(&pcb->indiceDeStack->retVar->offset, pcb_serial + offset, sizeof (pcb->indiceDeStack->retVar->offset));
	offset += sizeof (pcb->indiceDeStack->retVar->offset);
	memcpy(&pcb->indiceDeStack->retVar->size, pcb_serial + offset, sizeof (pcb->indiceDeStack->retVar->size));
	offset += sizeof (pcb->indiceDeStack->retVar->size);

	pcb->indiceDeEtiquetas = malloc(pcb->etiquetaSize);
	memcpy(pcb->indiceDeEtiquetas, pcb_serial + offset, pcb->etiquetaSize);
	offset += pcb->etiquetaSize;

	return pcb;
}

char *recvPCB(int sock_in){
	puts("Se recibe el PCB..");

	int stat, pack_size;
	char *pcb_serial;

	if ((stat = recv(sock_in, &pack_size, sizeof(int), 0)) <= 0){
		perror("Fallo de recv. error");
		return NULL;
	}

	printf("Paquete de size: %d\n", pack_size);

	pcb_serial = malloc(pack_size);
	if ((stat = recv(sock_in, pcb_serial, pack_size, 0)) <= 0){
		perror("Fallo de recv. error");
		return NULL;
	}

	return pcb_serial;
}

char *serializeByteRequest(tPCB *pcb, int *pack_size){

	int code_page = 0;
	int size_instr = pcb->indiceDeCodigo->offset - pcb->indiceDeCodigo->start;
	tPackHeader head_tmp = {.tipo_de_proceso = CPU, .tipo_de_mensaje = INSTRUC_GET};

	char *bytereq_serial;
	if ((bytereq_serial = malloc(sizeof(tPackByteReq))) == NULL){
		fprintf(stderr, "No se pudo malloquear\n");
		return NULL;
	}

	*pack_size = 0;
	memcpy(bytereq_serial, &head_tmp, HEAD_SIZE); 			// HEAD
	*pack_size += HEAD_SIZE;
	memcpy(bytereq_serial, &pcb->id, sizeof pcb->id);		// PID
	*pack_size += sizeof pcb->id;
	memcpy(bytereq_serial, &pcb->pc, sizeof pcb->pc);		// PC
	*pack_size += sizeof pcb->pc;
	memcpy(bytereq_serial, &code_page, sizeof code_page);	// CODE_PAGE
	*pack_size += sizeof code_page;
	memcpy(bytereq_serial, &pcb->indiceDeCodigo->start, sizeof pcb->indiceDeCodigo->offset); // OFFSET_BEGIN
	*pack_size += sizeof pcb->indiceDeCodigo->start;
	memcpy(bytereq_serial, &size_instr, sizeof size_instr); 		// SIZE
	*pack_size += sizeof size_instr;

	return bytereq_serial;
}

tPackSrcCode *recvSourceCode(int sock_in){

	int stat;
	tPackSrcCode *src_pack;
	if ((src_pack = malloc(sizeof *src_pack)) == NULL){
		perror("No se pudo mallocar espacio para el paquete src_pack. error");
		return NULL;
	}

	// recibimos el valor de size que va a tener el codigo fuente
	if ((stat = recv(sock_in, &src_pack->sourceLen, sizeof (unsigned long), 0)) <= 0){
		perror("El socket cerro la conexion o hubo fallo de recepcion. error");
		errno = FALLO_RECV;
		return NULL;
	}

	// hacemos espacio para el codigo fuente
	if ((src_pack->sourceCode = malloc(src_pack->sourceLen)) == NULL){
		perror("No se pudo mallocar espacio para el src_pack->sourceCode. error");
		return NULL;
	}

	if ((stat = recv(sock_in, src_pack->sourceCode, src_pack->sourceLen, 0)) <= 0){
		perror("El socket cerro la conexion o hubo fallo de recepcion. error");
		errno = FALLO_RECV;
		return NULL;
	}

	return src_pack;
}


tPackSrcCode *deserializeSrcCode(int sock_in){

	unsigned long bufferSize;
	char *bufferCode;
	int offset = 0;
	int stat;
	tPackSrcCode *line_pack;

	// recibimos el valor del largo del codigo fuente
	if ((stat = recv(sock_in, &bufferSize, sizeof bufferSize, 0)) == -1){
		perror("No se pudo recibir el size del codigo fuente. error");
		return NULL;
	}
	bufferSize++; // hacemos espacio para el '\0'

	// hacemos espacio para toda la estructura en serie
	if ((line_pack = malloc(HEAD_SIZE + sizeof (int) + bufferSize)) == NULL){
		perror("No se pudo mallocar para el src_pack");
		return NULL;
	}

	offset += sizeof (tPackHeader);
	memcpy(line_pack + offset, &bufferSize, sizeof bufferSize);
	offset += sizeof bufferSize;

	if ((bufferCode = malloc(bufferSize)) == NULL){
		perror("No se pudo almacenar memoria para el buffer del codigo fuente. error");
		return NULL;
	}

	// recibimos el codigo fuente
	if ((stat = recv(sock_in, bufferCode, bufferSize, 0)) == -1){
		perror("No se pudo recibir el size del codigo fuente. error");
		return NULL;
	}
	bufferCode[bufferSize -1] = '\0';

	memcpy(line_pack + offset, bufferCode, bufferSize);

	return line_pack;
}

void *serializeSrcCodeFromRecv(int sock_in, tPackHeader head, int *packSize){

	unsigned long bufferSize;
	void *bufferCode;
	int offset = 0;
	void *src_pack;

	int stat;

	// recibimos el valor del largo del codigo fuente
	if ((stat = recv(sock_in, &bufferSize, sizeof bufferSize, 0)) == -1){
		perror("No se pudo recibir el size del codigo fuente. error");
		return NULL;
	}

	// hacemos espacio para toda la estructura en serie
	if ((src_pack = malloc(HEAD_SIZE + sizeof (unsigned long) + bufferSize)) == NULL){
		perror("No se pudo mallocar para el src_pack");
		return NULL;
	}

	// copiamos el header, y el valor del bufferSize en el paquete
	memcpy(src_pack, &head, sizeof head);

	offset += sizeof head;
	memcpy(src_pack + offset, &bufferSize, sizeof bufferSize);
	offset += sizeof bufferSize;

	if ((bufferCode = malloc(bufferSize)) == NULL){
		perror("No se pudo almacenar memoria para el buffer del codigo fuente. error");
		return NULL;
	}

	// recibimos el codigo fuente
	if ((stat = recv(sock_in, bufferCode, bufferSize, 0)) == -1){
		perror("No se pudo recibir el size del codigo fuente. error");
		return NULL;
	}

	memcpy(src_pack + offset, bufferCode, bufferSize);

	// size total del paquete serializado
	*packSize = HEAD_SIZE + sizeof (unsigned long) + bufferSize;
	free(bufferCode);
	return src_pack;
}

char *serializePID(tPackPID *ppid){

	int off = 0;
	char *pid_serial;
	if ((pid_serial = malloc(sizeof *ppid)) == NULL){
		perror("No se pudo crear espacio de memoria para PID serial. error");
		return NULL;
	}

	memcpy(pid_serial + off, &ppid->head.tipo_de_proceso, sizeof ppid->head.tipo_de_proceso);
	off += sizeof ppid->head.tipo_de_proceso;
	memcpy(pid_serial + off, &ppid->head.tipo_de_mensaje, sizeof ppid->head.tipo_de_mensaje);
	off += sizeof ppid->head.tipo_de_mensaje;
	memcpy(pid_serial + off, &ppid->pid, sizeof ppid->pid);
	off += sizeof ppid->pid;

	return pid_serial;
}

char *serializarPCBACpu(tPackPCBSimul *pcb){

	int offset = 0;

	char *serial_pcb = malloc(sizeof pcb->head + sizeof pcb->exit+ sizeof pcb->pages+ sizeof pcb->pid+ sizeof pcb->pc);
	if (serial_pcb == NULL){
		perror("No se pudo mallocar el serial_pcb. error");
		return NULL;
	}

	memcpy(serial_pcb, &pcb->head, sizeof pcb->head);
	offset += sizeof pcb->head;
	memcpy(serial_pcb + offset, &pcb->pid, sizeof pcb->pid);
	offset += sizeof pcb->pid;
	memcpy(serial_pcb + offset, &pcb->pc, sizeof pcb->pc);
	offset += sizeof pcb->pc;
	memcpy(serial_pcb + offset, &pcb->pages, sizeof pcb->pages);
	offset += sizeof pcb->pages;
	memcpy(serial_pcb + offset, &pcb->exit, sizeof pcb->exit);
	offset += sizeof pcb->exit;

	return serial_pcb;
}



/****** Funciones generales sobre Paquetes ******/

tPackPCBSimul *empaquetarPCBconStruct(tPackHeader head, tPCB *pcb){

	tPackPCBSimul *pack_pcb = malloc(sizeof *pack_pcb);
	pack_pcb->head.tipo_de_proceso = KER;
	pack_pcb->head.tipo_de_mensaje = PCB_EXEC;
	pack_pcb->pid   = pcb->id;
	pack_pcb->pc    = pcb->pc;
	pack_pcb->pages = pcb->paginasDeCodigo;
	pack_pcb->exit  = pcb->exitCode;

	return pack_pcb;
}
