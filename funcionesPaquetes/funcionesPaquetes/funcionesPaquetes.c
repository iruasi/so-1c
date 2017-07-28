#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>

#include <commons/collections/list.h>

#include <funcionesCompartidas/funcionesCompartidas.h>
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

int contestar2ProcAProc(tPackHeader h, int val1, int val2, int sock){

	int stat, pack_size;
	char *hs_serial;

	tHShake2ProcAProc *h_shake = malloc(sizeof *h_shake);
	h_shake->head.tipo_de_proceso = h.tipo_de_proceso;
	h_shake->head.tipo_de_mensaje = h.tipo_de_mensaje;
	h_shake->val1 = val1;
	h_shake->val2 = val2;

	if ((hs_serial = serialize2ProcAProc(h_shake, &pack_size)) == NULL){
		puts("Fallo la serializacion de handshake 2ProcAProc");
		return FALLO_SERIALIZAC;
	}

	if((stat = send(sock, hs_serial, pack_size, 0)) == -1)
		perror("Error de envio informacion de 2ProcAProc. error");

	return stat;
}

int contestarProcAProc(tPackHeader head, int val, int sock){

	int stat, pack_size;
	char *hs_serial;

	tHShakeProcAProc h_shake;
	memcpy(&h_shake.head, &head, HEAD_SIZE);
	h_shake.val = val;

	pack_size = 0;
	if ((hs_serial = serializeProcAProc(&h_shake, &pack_size)) == NULL){
		puts("No se pudo serializar el handshake proceso a proceso");
		return FALLO_SERIALIZAC;
	}

	if((stat = send(sock, hs_serial, pack_size, 0)) == -1)
		perror("Error de envio informacion de proceso a proceso. error");

	free(hs_serial);
	return stat;
}

int recibirInfo2ProcAProc(int sock, tPackHeader h, int *val1, int *val2){

	int stat;
	char *info_serial;
	tPackHeader head;

	if ((stat = recv(sock, &head, HEAD_SIZE, 0)) == -1){
		perror("Fallo recepcion de info de Memoria. error");
		return FALLO_RECV;
	}

	if (head.tipo_de_proceso != h.tipo_de_proceso || head.tipo_de_mensaje != h.tipo_de_mensaje){
		printf("El paquete recibido no era el esperado! Proceso: %d, Mensaje: %d\n",
				head.tipo_de_proceso, head.tipo_de_mensaje);
		return FALLO_GRAL;
	}

	if ((info_serial = recvGeneric(sock)) == NULL)
		return FALLO_GRAL;

	memcpy(val1, info_serial, sizeof(int));
	memcpy(val2, info_serial + sizeof(int), sizeof(int));

	free(info_serial);
	return 0;
}

int recibirInfoProcSimple(int sock, tPackHeader h_esp, int *var){

	int stat;
	char *info_serial;
	tPackHeader head;

	if ((stat = recv(sock, &head, HEAD_SIZE, 0)) == -1){
		perror("Fallo recepcion de info de Proceso. error");
		return FALLO_RECV;
	}

	if (head.tipo_de_proceso != h_esp.tipo_de_proceso || head.tipo_de_mensaje != h_esp.tipo_de_mensaje){
		printf("El paquete recibido no era el esperado! Proceso: %d, Mensaje: %d\n",
				head.tipo_de_proceso, head.tipo_de_mensaje);
		return FALLO_GRAL;
	}

	if ((info_serial = recvGeneric(sock)) == NULL){
		puts("Fallo recepcion generica");
		return FALLO_GRAL;
	}

	memcpy(var, info_serial, sizeof(int));
	free(info_serial);
	return 0;

}

/****** Definiciones de [De]Serializaciones Handshakes especiales ******/

char *serialize2ProcAProc(tHShake2ProcAProc *h_shake, int *pack_size){

	char *hs_serial;

	if ((hs_serial = malloc(sizeof *h_shake + sizeof(int))) == NULL){
		printf("No se pudieron mallocar %d bytes para el handshake de Memoria a Kernel\n", sizeof *h_shake);
		return NULL;
	}

	*pack_size = 0;
	memcpy(hs_serial + *pack_size, &h_shake->head, HEAD_SIZE);
	*pack_size += HEAD_SIZE;

	*pack_size += sizeof(int);

	memcpy(hs_serial + *pack_size, &h_shake->val1, sizeof(int));
	*pack_size += sizeof(int);
	memcpy(hs_serial + *pack_size, &h_shake->val2, sizeof(int));
	*pack_size += sizeof(int);

	memcpy(hs_serial + HEAD_SIZE, pack_size, sizeof(int));

	return hs_serial;
}

char *serializeProcAProc(tHShakeProcAProc *h_shake, int *pack_size){

	char *hs_serial;
	if ((hs_serial = malloc(sizeof *h_shake + sizeof(int))) == NULL){
		printf("No se pudieron mallocar %d bytes para handshake de Memoria a CPU\n", sizeof *h_shake);
		return NULL;
	}

	*pack_size = 0;
	memcpy(hs_serial + *pack_size, &h_shake->head, HEAD_SIZE);
	*pack_size += HEAD_SIZE;

	*pack_size += sizeof(int);

	memcpy(hs_serial + *pack_size, &h_shake->val, sizeof(int));
	*pack_size += sizeof (int);

	memcpy(hs_serial + HEAD_SIZE, pack_size, sizeof(int));

	return hs_serial;
}

char *recvGenericWFlags(int sock_in, int flags){
	//printf("Se recibe el paquete serializado, usando flags %x\n", flags);

	int stat, pack_size;
	char *p_serial;

	if ((stat = recv(sock_in, &pack_size, sizeof(int), flags)) == -1){
		perror("Fallo de recv. error");
		return NULL;

	} else if (stat == 0){
		printf("El proceso del socket %d se desconecto. No se pudo completar recvGenerico\n", sock_in);
		return NULL;
	}

	pack_size -= (HEAD_SIZE + sizeof(int)); // ya se recibieron estas dos cantidades
	//printf("Paquete de size: %d\n", pack_size);

	if ((p_serial = malloc(pack_size)) == NULL){
		printf("No se pudieron mallocar %d bytes para paquete generico\n", pack_size);
		return NULL;
	}

	if ((stat = recv(sock_in, p_serial, pack_size, flags)) == -1){
		perror("Fallo de recv. error");
		return NULL;

	} else if (stat == 0){
		printf("El proceso del socket %d se desconecto. No se pudo completar recvGenerico\n", sock_in);
		return NULL;
	}

	return p_serial;
}

char *recvGeneric(int sock_in){
	return recvGenericWFlags(sock_in, 0);
}

/****** Definiciones de [De]Serializaciones Regulares ******/

char *serializeHeader(tPackHeader head, int *pack_size){

	char *h_serial = malloc(HEAD_SIZE);
	*pack_size = HEAD_SIZE;
	return memcpy(h_serial, &head, HEAD_SIZE);
}

char *serializeBytes(tPackHeader head, char* buffer, int buffer_size, int *pack_size){

	char *bytes_serial;
	if ((bytes_serial = malloc(HEAD_SIZE + sizeof(int) + sizeof(int) + buffer_size)) == NULL){
		fprintf(stderr, "No se pudo mallocar espacio para paquete de bytes\n");
		return NULL;
	}

	*pack_size = 0;
	memcpy(bytes_serial + *pack_size, &head, HEAD_SIZE);
	*pack_size += HEAD_SIZE;

	// hacemos lugar para el payload_size
	*pack_size += sizeof(int);

	memcpy(bytes_serial + *pack_size, &buffer_size, sizeof buffer_size);
	*pack_size += sizeof (int);
	memcpy(bytes_serial + *pack_size, buffer, buffer_size);
	*pack_size += buffer_size;

	memcpy(bytes_serial + HEAD_SIZE, pack_size, sizeof(int));

	return bytes_serial;
}

tPackBytes *deserializeBytes(char *bytes_serial){

	int off;
	tPackBytes *pbytes;

	if ((pbytes = malloc(sizeof *pbytes)) == NULL){
		fprintf(stderr, "No se pudo mallocar espacio para paquete de bytes\n");
		return NULL;
	}

	off = 0;
	memcpy(&pbytes->bytelen, bytes_serial + off, sizeof (int));
	off += sizeof (int);

	if ((pbytes->bytes = malloc(pbytes->bytelen)) == NULL){
		printf("No se pudieron mallocar %d bytes al Paquete De Bytes\n", pbytes->bytelen);
		return NULL;
	}

	memcpy(pbytes->bytes, bytes_serial + off, pbytes->bytelen);
	off += pbytes->bytelen;

	return pbytes;
}

char *serializePCB(tPCB *pcb, tPackHeader head, int *pack_size){

	int off = 0;
	char *pcb_serial;

	size_t ctesInt_size         = CTES_INT_PCB * sizeof (int);
	size_t indiceCod_size       = pcb->cantidad_instrucciones * 2 * sizeof(int);
	size_t indiceStack_size     = sumarPesosStack(pcb->indiceDeStack) + sizeof(int) +
								  list_size(pcb->indiceDeStack) * 2 * sizeof(int);
	size_t indiceEtiquetas_size = pcb->etiquetas_size;

	if ((pcb_serial = malloc(HEAD_SIZE + sizeof(int) + ctesInt_size + indiceCod_size +
			                 indiceStack_size + indiceEtiquetas_size)) == NULL){
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
	memcpy(pcb_serial + off, &pcb->etiquetas_size, sizeof (int));
	off += sizeof (int);
	memcpy(pcb_serial + off, &pcb->cantidad_etiquetas, sizeof (int));
	off += sizeof (int);
	memcpy(pcb_serial + off, &pcb->cantidad_funciones, sizeof (int));
	off += sizeof (int);
	memcpy(pcb_serial + off, &pcb->proxima_rafaga, sizeof (int));
	off += sizeof (int);
	memcpy(pcb_serial + off, &pcb->cantidad_instrucciones, sizeof (int));
	off += sizeof (int);
	memcpy(pcb_serial + off, &pcb->estado_proc, sizeof (int));
	off += sizeof (int);
	memcpy(pcb_serial + off, &pcb->contextoActual, sizeof (int));
	off += sizeof (int);
	memcpy(pcb_serial + off, &pcb->exitCode, sizeof (int));
	off += sizeof (int);
	memcpy(pcb_serial + off, &pcb->rafagasEjecutadas, sizeof (int));
	off += sizeof (int);
	memcpy(pcb_serial + off, &pcb->cantSyscalls, sizeof (int));
	off += sizeof (int);
	// serializamos indice de codigo
	memcpy(pcb_serial + off, pcb->indiceDeCodigo, indiceCod_size);
	off += indiceCod_size;

	// serializamos indice de stack
	*pack_size = 0;
	char *stack_serial = serializarStack(pcb, indiceStack_size, pack_size);
	memcpy(pcb_serial + off, stack_serial, *pack_size);
	off += *pack_size;

	// serializamos indice de etiquetas
	if (pcb->cantidad_etiquetas || pcb->cantidad_funciones){
		memcpy(pcb_serial + off, pcb->indiceDeEtiquetas, pcb->etiquetas_size);
		off += pcb->etiquetas_size;
	}

	memcpy(pcb_serial + HEAD_SIZE, &off, sizeof(int));
	*pack_size = off;

	free(stack_serial);
	return pcb_serial;
}


char *serializarStack(tPCB *pcb, int pesoStack, int *pack_size){

	char *stack_serial;
	if ((stack_serial = malloc(pesoStack)) == NULL){
		puts("No se pudo mallocar espacio para el stack serializado");
		return NULL;
	}

	indiceStack *stack;
	posicionMemoria *arg;
	posicionMemoriaId *var;
	int args_size, vars_size, stack_size;
	int i, j;
	stack_size = list_size(pcb->indiceDeStack);

	*pack_size = 0;
	memcpy(stack_serial + *pack_size, &stack_size, sizeof(int));
	*pack_size += sizeof (int);

	if (!stack_size)
		return stack_serial; // no hay stack que serializar

	for (i = 0; i < stack_size; ++i){ 		// para cada piso del stack...
		stack = list_get(pcb->indiceDeStack, i);

		args_size = list_size(stack->args);
		memcpy(stack_serial + *pack_size, &args_size, sizeof(int));
		*pack_size += sizeof(int);
		for(j = 0; j < args_size; j++){		// serializamos cada arg...
			arg = list_get(stack->args, j);
			memcpy(stack_serial + *pack_size, &arg, sizeof (posicionMemoria));
			*pack_size += sizeof (posicionMemoria);
		}

		vars_size = list_size(stack->vars);
		memcpy(stack_serial + *pack_size, &vars_size, sizeof(int));
		*pack_size += sizeof (int);
		for(j = 0; j < vars_size; j++){		// serializamos cada var...
			var = list_get(stack->vars, j);
			memcpy(stack_serial + *pack_size, var, sizeof (posicionMemoriaId));
			*pack_size += sizeof (posicionMemoriaId);
		}

		memcpy(stack_serial + *pack_size, &stack->retPos, sizeof(int));
		*pack_size += sizeof (int);

		memcpy(stack_serial + *pack_size, &stack->retVar, sizeof(posicionMemoria));
		*pack_size += sizeof (posicionMemoria);
	}

	return stack_serial;
}


tPCB *deserializarPCB(char *pcb_serial){
	//puts("Deserializamos PCB");

	int offset = 0;
	size_t indiceCod_size;
	tPCB *pcb;

	if ((pcb = malloc(sizeof *pcb)) == NULL){
		fprintf(stderr, "Fallo malloc\n");
		return NULL;
	}

	memcpy(&pcb->id, pcb_serial + offset, sizeof(int));
	offset += sizeof(int);
	memcpy(&pcb->pc, pcb_serial + offset, sizeof(int));
	offset += sizeof(int);
	memcpy(&pcb->paginasDeCodigo, pcb_serial + offset, sizeof(int));
	offset += sizeof(int);
	memcpy(&pcb->etiquetas_size, pcb_serial + offset, sizeof(int));
	offset += sizeof(int);
	memcpy(&pcb->cantidad_etiquetas, pcb_serial + offset, sizeof(int));
	offset += sizeof(int);
	memcpy(&pcb->cantidad_funciones, pcb_serial + offset, sizeof(int));
	offset += sizeof(int);
	memcpy(&pcb->proxima_rafaga, pcb_serial + offset, sizeof(int));
	offset += sizeof(int);
	memcpy(&pcb->cantidad_instrucciones, pcb_serial + offset, sizeof(int));
	offset += sizeof(int);
	memcpy(&pcb->estado_proc, pcb_serial + offset, sizeof(int));
	offset += sizeof(int);
	memcpy(&pcb->contextoActual, pcb_serial + offset, sizeof(int));
	offset += sizeof(int);
	memcpy(&pcb->exitCode, pcb_serial + offset, sizeof(int));
	offset += sizeof(int);
	memcpy(&pcb->rafagasEjecutadas, pcb_serial + offset, sizeof(int));
	offset += sizeof(int);
	memcpy(&pcb->cantSyscalls, pcb_serial + offset, sizeof(int));
	offset += sizeof(int);

	indiceCod_size = pcb->cantidad_instrucciones * 2 * sizeof(int);
	if ((pcb->indiceDeCodigo = malloc(indiceCod_size)) == NULL){
		fprintf(stderr, "Fallo malloc\n");
		return NULL;
	}

	memcpy(pcb->indiceDeCodigo, pcb_serial + offset, indiceCod_size);
	offset += indiceCod_size;

	deserializarStack(pcb, pcb_serial, &offset);

	pcb->indiceDeEtiquetas = NULL; // si hay etiquetas o funciones las memcpy'amos
	if (pcb->cantidad_etiquetas || pcb->cantidad_funciones){
		pcb->indiceDeEtiquetas = malloc(pcb->etiquetas_size);
		memcpy(pcb->indiceDeEtiquetas, pcb_serial + offset, pcb->etiquetas_size);
		offset += pcb->etiquetas_size;
	}


	return pcb;
}

void deserializarStack(tPCB *pcb, char *pcb_serial, int *offset){
	//puts("Deserializamos stack..");

	pcb->indiceDeStack = list_create();

	int stack_depth;
	memcpy(&stack_depth, pcb_serial + *offset, sizeof (int));
	*offset += sizeof(int);

	if (stack_depth == 0)
		return;

	int arg_depth, var_depth;
	posicionMemoria retVar;
	int retPos;

	int i, j;
	for (i = 0; i < stack_depth; ++i){
		indiceStack *stack = crearStackVacio();

		memcpy(&arg_depth, pcb_serial + *offset, sizeof(int));
		*offset += sizeof(int);
		for (j = 0; j < arg_depth; j++){
			posicionMemoria *arg = malloc(sizeof *arg);
			memcpy((arg + j), pcb_serial + *offset, sizeof(posicionMemoria));
			*offset += sizeof(posicionMemoria);
			list_add(stack->args, arg);
		}

		memcpy(&var_depth, pcb_serial + *offset, sizeof(int));
		*offset += sizeof(int);
		for (j = 0; j < var_depth; j++){
			posicionMemoriaId *var = malloc(sizeof *var);
			memcpy(var, pcb_serial + *offset, sizeof(posicionMemoriaId));
			*offset += sizeof(posicionMemoriaId);
			list_add(stack->vars, var);
		}

		memcpy(&retPos, pcb_serial + *offset, sizeof (int));
		*offset += sizeof(int);
		stack->retPos = retPos;

		memcpy(&retVar, pcb_serial + *offset, sizeof(posicionMemoria));
		*offset += sizeof(posicionMemoria);
		stack->retVar = retVar;

		list_add(pcb->indiceDeStack, stack);
	}
}

char *serializeByteRequest(tPackByteReq *pbr, int *pack_size){

	char *byterq_serial;
	if ((byterq_serial = malloc(sizeof(int) + sizeof(tPackByteReq))) == NULL){
		printf("No se pudieron mallocar %d bytes para el Byte Request serializado\n",
				sizeof(int) + sizeof(tPackByteReq));
		return NULL;
	}

	*pack_size = 0;
	memcpy(byterq_serial + *pack_size, &pbr->head, HEAD_SIZE);
	*pack_size += HEAD_SIZE;

	*pack_size += sizeof(int);

	memcpy(byterq_serial + *pack_size, &pbr->pid, sizeof(int));
	*pack_size += sizeof(int);
	memcpy(byterq_serial + *pack_size, &pbr->page, sizeof(int));
	*pack_size += sizeof(int);
	memcpy(byterq_serial + *pack_size, &pbr->offset, sizeof(int));
	*pack_size += sizeof(int);
	memcpy(byterq_serial + *pack_size, &pbr->size, sizeof(int));
	*pack_size += sizeof(int);

	memcpy(byterq_serial + HEAD_SIZE, pack_size, sizeof(int));

	return byterq_serial;
}

tPackByteReq *deserializeByteRequest(char *byterq_serial){

	int off;
	tPackByteReq *pbrq;
	if ((pbrq = malloc(sizeof *pbrq)) == NULL){
		fprintf(stderr, "No se pudo mallocar espacio para el paquete de bytes deserializado\n");
	}

	off = 0;
	memcpy(&pbrq->pid, byterq_serial + off, sizeof(int));
	off += sizeof(int);
	memcpy(&pbrq->page, byterq_serial + off, sizeof(int));
	off += sizeof(int);
	memcpy(&pbrq->offset, byterq_serial + off, sizeof(int));
	off += sizeof(int);
	memcpy(&pbrq->size, byterq_serial + off, sizeof(int));
	off += sizeof(int);

	return pbrq;
}

char *serializeByteAlmacenamiento(tPackByteAlmac *pbal, int* pack_size){

	int size_pbal = sizeof(tPackByteAlmac) + pbal->size;

	char *pbyte_al;
	if ((pbyte_al = malloc(size_pbal)) == NULL){
		printf("No se pudo mallocar %d bytes para el paquete de bytes almacenamiento\n", size_pbal);
		return NULL;
	}

	*pack_size = 0;
	memcpy(pbyte_al + *pack_size, &pbal->head, HEAD_SIZE);
	*pack_size += HEAD_SIZE;

	// dejamos espacio para el payload_size
	*pack_size += sizeof(int);

	memcpy(pbyte_al + *pack_size, &pbal->pid, sizeof(int));
	*pack_size += sizeof(int);
	memcpy(pbyte_al + *pack_size, &pbal->page, sizeof(int));
	*pack_size += sizeof(int);
	memcpy(pbyte_al + *pack_size, &pbal->offset, sizeof(int));
	*pack_size += sizeof(int);
	memcpy(pbyte_al + *pack_size, &pbal->size, sizeof(int));
	*pack_size += sizeof(int);
	memcpy(pbyte_al + *pack_size, pbal->bytes, pbal->size);
	*pack_size += pbal->size;

	memcpy(pbyte_al + HEAD_SIZE, pack_size, sizeof(int));

	return pbyte_al;
}

tPackByteAlmac *deserializeByteAlmacenamiento(char *pbal_serial){
	//puts("deserializamos bytes almacenamiento");

	int off;
	tPackByteAlmac *pbal;
	if ((pbal = malloc(sizeof *pbal)) == NULL){
		printf("No se pudo mallocar %d bytes para el paquete de bytes deserializado\n", sizeof *pbal);
		return NULL;
	}

	off = 0;
	memcpy(&pbal->pid,    pbal_serial + off, sizeof (int));
	off += sizeof (int);
	memcpy(&pbal->page,   pbal_serial + off, sizeof (int));
	off += sizeof (int);
	memcpy(&pbal->offset, pbal_serial + off, sizeof (int));
	off += sizeof (int);
	memcpy(&pbal->size,   pbal_serial + off, sizeof (int));
	off += sizeof (int);

	if ((pbal->bytes = malloc(pbal->size)) == NULL){
		printf("No se pudieron mallocar %d bytes para el paquete de Almacenamiento\n", pbal->size);
		freeAndNULL((void **) &pbal);
		return NULL;
	}

	memcpy(pbal->bytes,  pbal_serial + off, pbal->size);
	off += pbal->size;

	return pbal;
}

char *serializeSrcCode(tPackSrcCode *src_code, int *pack_size){

	char *src_serial;
	if ((src_serial = malloc(HEAD_SIZE + sizeof(int) + sizeof src_code->bytelen + src_code->bytelen)) == NULL){
		perror("No se pudo mallocar el src_serial. error");
		return NULL;
	}

	*pack_size = 0;
	memcpy(src_serial + *pack_size, &src_code->head, HEAD_SIZE);
	*pack_size += HEAD_SIZE;

	*pack_size += sizeof(int);

	memcpy(src_serial + *pack_size, &src_code->bytelen, sizeof src_code->bytelen);
	*pack_size += sizeof src_code->bytelen;
	memcpy(src_serial + *pack_size, src_code->bytes, src_code->bytelen);
	*pack_size += src_code->bytelen;

	return src_serial;
}

char *serializeVal(tPackVal *pval, int *pack_size){

	char *val_serial;
	if ((val_serial = malloc(sizeof(int) + sizeof *pval)) == NULL){
		perror("No se pudo crear espacio de memoria para PID serial. error");
		return NULL;
	}

	*pack_size = 0;
	memcpy(val_serial + *pack_size, &pval->head, HEAD_SIZE);
	*pack_size += HEAD_SIZE;

	*pack_size += sizeof(int);

	memcpy(val_serial + *pack_size, &pval->val, sizeof(int));
	*pack_size += sizeof (int);

	memcpy(val_serial + HEAD_SIZE, pack_size, sizeof(int));

	return val_serial;
}

tPackPID *deserializeVal(char *val_serial){

	tPackVal *pval;
	if ((pval = malloc(sizeof *pval)) == NULL){
		printf("No se pudieron mallocar %d bytes para paquete PID\n", sizeof *pval);
		return NULL;
	}

	memcpy(&pval->val, val_serial, sizeof(int));

	return pval;
}

char *serializePIDPaginas(tPackPidPag *ppidpag, int *pack_size){

	char *pidpag_serial;

	if ((pidpag_serial = malloc(sizeof *ppidpag)) == NULL){
		fprintf(stderr, "No se pudo crear espacio de memoria para pid_paginas serial\n");
		return NULL;
	}

	*pack_size = 0;
	memcpy(pidpag_serial + *pack_size, &ppidpag->head, HEAD_SIZE);
	*pack_size += HEAD_SIZE;

	*pack_size += sizeof(int);

	memcpy(pidpag_serial + *pack_size, &ppidpag->pid, sizeof (int));
	*pack_size += sizeof (int);
	memcpy(pidpag_serial + *pack_size, &ppidpag->pageCount, sizeof (int));
	*pack_size += sizeof (int);

	memcpy(pidpag_serial + HEAD_SIZE, pack_size, sizeof(int));

	return pidpag_serial;
}

tPackPidPag *deserializePIDPaginas(char *pidpag_serial){

	int off;
	tPackPidPag *ppidpag;
	if ((ppidpag = malloc(sizeof *ppidpag)) == NULL){
		printf("Fallo malloc de %d bytes para packPIDPag\n", sizeof *ppidpag);
		return NULL;
	}

	off = 0;
	memcpy(&ppidpag->pid, pidpag_serial + off, sizeof(int));
	off += sizeof(int);
	memcpy(&ppidpag->pageCount, pidpag_serial + off, sizeof(int));
	off += sizeof(int);

	return ppidpag;
}



/****** Definiciones de [De]Serializaciones CPU ******/

char *serializeAbrir(tPackAbrir *abrir, int *pack_size){

	tPackHeader head = {.tipo_de_proceso = CPU, .tipo_de_mensaje = ABRIR};

	char *abrir_serial;
	abrir_serial = malloc(HEAD_SIZE + sizeof(int) + sizeof(int) + abrir->longitudDireccion + sizeof abrir->flags);

	*pack_size = 0;
	memcpy(abrir_serial + *pack_size, &head, HEAD_SIZE);
	*pack_size += HEAD_SIZE;

	*pack_size += sizeof(int);

	memcpy(abrir_serial + *pack_size, &abrir->longitudDireccion,  sizeof(int));
	*pack_size += sizeof(int);
	memcpy(abrir_serial + *pack_size, abrir->direccion, abrir->longitudDireccion);
	*pack_size += abrir->longitudDireccion;
	memcpy(abrir_serial + *pack_size, &abrir->flags, sizeof (abrir->flags));
	*pack_size += sizeof (abrir->flags);

	memcpy(abrir_serial + HEAD_SIZE, pack_size, sizeof(int));

	return abrir_serial;
}

char *serializeMoverCursor(t_descriptor_archivo descriptor_archivo, t_valor_variable posicion, int *pack_size){

	tPackHeader head = {.tipo_de_proceso = CPU, .tipo_de_mensaje = MOVERCURSOR};

	char *mov_serial;
	mov_serial = malloc(HEAD_SIZE + sizeof(int) + sizeof descriptor_archivo + sizeof posicion);

	*pack_size = 0;
	memcpy(mov_serial + *pack_size, &head, HEAD_SIZE);
	*pack_size += HEAD_SIZE;

	*pack_size += sizeof(int);

	memcpy(mov_serial + *pack_size, &descriptor_archivo, sizeof descriptor_archivo);
	*pack_size += sizeof descriptor_archivo;
	memcpy(mov_serial + *pack_size, &posicion, sizeof posicion);
	*pack_size += sizeof posicion;

	memcpy(mov_serial + HEAD_SIZE, pack_size, sizeof(int));

	return mov_serial;
}

tPackCursor *deserializeMoverCursor(char *cursor_serial){

	tPackCursor * cursor = malloc(sizeof *cursor);

	int off = 0;
	memcpy(&cursor->fd, cursor_serial + off, sizeof(int));
	off += sizeof(int);
	memcpy(&cursor->posicion, cursor_serial + off , sizeof(int));
	off += sizeof(int);

	return cursor;
}


char *serializeEscribir(t_descriptor_archivo descriptor_archivo, void* informacion, t_valor_variable tamanio, int *pack_size){

	tPackHeader head = {.tipo_de_proceso = CPU, .tipo_de_mensaje = ESCRIBIR};

	char *escr_serial;
	escr_serial = malloc(HEAD_SIZE + sizeof(int) + sizeof descriptor_archivo + sizeof tamanio + tamanio);

	*pack_size = 0;
	memcpy(escr_serial + *pack_size, &head, HEAD_SIZE);
	*pack_size += HEAD_SIZE;

	*pack_size += sizeof(int);

	memcpy(escr_serial + *pack_size, &descriptor_archivo, sizeof descriptor_archivo);
	*pack_size += sizeof descriptor_archivo;
	memcpy(escr_serial + *pack_size, &tamanio, sizeof tamanio);
	*pack_size += sizeof tamanio;
	memcpy(escr_serial + *pack_size, informacion, tamanio);
	*pack_size += tamanio;

	memcpy(escr_serial + HEAD_SIZE, pack_size, sizeof(int));

	return escr_serial;
}

tPackRW *deserializeEscribir(char *escr_serial){

	tPackRW *escr;
	escr = malloc(sizeof *escr);
	int off = 0;

	memcpy(&escr->fd, escr_serial + off, sizeof(int));
	off += sizeof(int);
	memcpy(&escr->tamanio, escr_serial + off, sizeof(int));
	off += sizeof(int);
	escr->info = malloc(escr->tamanio);
	memcpy(escr->info, escr_serial + off, escr->tamanio);
	off += escr->tamanio;

	return escr;
}


char *serializeRW(tPackHeader head, tPackRW *read_write, int *pack_size){

	char *rw_serial = malloc(HEAD_SIZE + sizeof(int) + sizeof *read_write + read_write->tamanio);

	*pack_size = 0;
	memcpy(rw_serial + *pack_size, &head, HEAD_SIZE);
	*pack_size += HEAD_SIZE;

	*pack_size += sizeof(int);

	memcpy(rw_serial + *pack_size, &read_write->fd, sizeof(int));
	*pack_size += sizeof(int);
	memcpy(rw_serial + *pack_size, &read_write->tamanio, sizeof(read_write->tamanio));
	*pack_size += sizeof(read_write->tamanio);
	memcpy(rw_serial + *pack_size, read_write->info, read_write->tamanio),
	*pack_size += read_write->tamanio;

	memcpy(rw_serial + HEAD_SIZE, pack_size, sizeof(int));

	return rw_serial;
}

char *serializeLeer(tPackLeer *read, int *pack_size){

	tPackHeader head = {.tipo_de_proceso = CPU, .tipo_de_mensaje = LEER};

	char * r_serial = malloc(HEAD_SIZE + 3 * sizeof(int));

	*pack_size = 0;
	memcpy(r_serial + *pack_size, &head, HEAD_SIZE);
	*pack_size += HEAD_SIZE;

	*pack_size += sizeof(int);

	memcpy(r_serial + *pack_size, &read->fd, sizeof(int));
	*pack_size += sizeof(int);
	memcpy(r_serial + *pack_size, &read->size, sizeof(int));
	*pack_size += sizeof(int);

	memcpy(r_serial + HEAD_SIZE, pack_size, sizeof(int));

	return r_serial;
}

tPackLeer *deserializeLeer(char *leer_serial){

	int off = 0;
	tPackLeer *p_leer = malloc(sizeof *p_leer);

	memcpy(&p_leer->fd, leer_serial + off, sizeof(int));
	off += sizeof(int);
	memcpy(&p_leer->size, leer_serial + off, sizeof(int));
	off += sizeof(int);

	return p_leer;
}

tPackRW * deserializeRW(char * rw_serial){
	tPackRW * read_write = malloc(sizeof *read_write);

	int off = 0;

	memcpy(&read_write->fd, off + rw_serial, sizeof(int));
	off += sizeof(int);
	memcpy(&read_write->tamanio, off + rw_serial, sizeof(read_write->tamanio));
	off += sizeof(read_write->tamanio);
	read_write->info = malloc(read_write->tamanio);
	memcpy(read_write->info, off + rw_serial, read_write->tamanio);
	off += read_write->tamanio;

	return read_write;
}

char *serializeValorYVariable(tPackHeader head, t_valor_variable valor, t_nombre_compartida variable, int *pack_size){
// variable ya tiene un '\0' al final

	int varlen = strlen(variable) + 1;

	char *valor_serial;
	int m_size = HEAD_SIZE + sizeof(int) + sizeof valor + sizeof(int) + varlen;
	if ((valor_serial = malloc(m_size)) == NULL){
		printf("No se pudieron mallocar %d bytes para valor y variable serializados\n", m_size);
		return NULL;
	}

	*pack_size = 0;
	memcpy(valor_serial + *pack_size, &head, HEAD_SIZE);
	*pack_size += HEAD_SIZE;

	*pack_size += sizeof(int);

	memcpy(valor_serial + *pack_size, &valor, sizeof valor);
	*pack_size += sizeof valor;
	memcpy(valor_serial + *pack_size, &varlen, sizeof(int));
	*pack_size += sizeof(int);
	memcpy(valor_serial + *pack_size, variable, varlen);
	*pack_size += varlen;

	memcpy(valor_serial + HEAD_SIZE, pack_size, sizeof(int));

	return valor_serial;
}

tPackValComp *deserializeValorYVariable(char *valor_serial){

	int off, nomlen;
	tPackValComp *val_comp;

	if ((val_comp = malloc(sizeof *val_comp)) == NULL){
		printf("No se pudieron mallocar %d bytes para valor y variable\n", sizeof *val_comp);
		return NULL;
	}

	off = 0;
	memcpy(&val_comp->val, valor_serial + off, sizeof(t_valor_variable));
	off += sizeof(val_comp->val);
	memcpy(&nomlen, valor_serial + off, sizeof(int));
	off += sizeof(int);
	val_comp->nom = malloc(nomlen);
	memcpy(val_comp->nom, valor_serial + off, nomlen);
	off += nomlen;

	return val_comp;
}


tPackAbrir * deserializeAbrir(char *abrir_serial){
	tPackAbrir * abrir = malloc(sizeof *abrir);
	int off = 0;

	memcpy(&abrir->longitudDireccion, abrir_serial + off, sizeof(int));
	off += sizeof(int);
	abrir->direccion = malloc(abrir->longitudDireccion);
	memcpy(abrir->direccion,abrir_serial + off, abrir->longitudDireccion);
	off += abrir->longitudDireccion;
	memcpy(&abrir->flags, abrir_serial + off, sizeof abrir->flags);
	off += sizeof abrir->flags;

	return abrir;

}


char * serializeFileDescriptor(tPackFS * fileSystem,int *pack_size){


	char *file_serial;
	file_serial = malloc(HEAD_SIZE + sizeof(int) + sizeof(int) + sizeof(int));
	tPackHeader head = {.tipo_de_proceso = CPU, .tipo_de_mensaje = ENTREGO_FD};
	*pack_size = 0;
	memcpy(pack_size + *file_serial,&head,HEAD_SIZE);
	*pack_size += HEAD_SIZE;

	*pack_size += sizeof(int);

	memcpy(pack_size + *file_serial,&fileSystem->fd,sizeof(int));
	*pack_size += sizeof(int);
	memcpy(pack_size + *file_serial,&fileSystem->cantidadOpen,sizeof(int));
	*pack_size += sizeof(int);

	memcpy(file_serial + HEAD_SIZE,pack_size,sizeof(int));

	return file_serial;
}
tPackFS * deserializeFileDescriptor(char * aux_serial){
	tPackFS * aux = malloc(sizeof *aux);

	int off = 0;

	memcpy(&aux->fd, aux_serial + off, sizeof(int));
	off += sizeof(int);
	memcpy(&aux->cantidadOpen, aux_serial + off, sizeof(int));
	off += sizeof(int);

	return aux;
}


char *serializeLeerFS2(tPackHeader head, t_direccion_archivo path, t_puntero cursor, int size, int *pack_size){

	int dirSize = strlen(path) + 1;
	char *leer_serial = malloc(HEAD_SIZE + sizeof(int) + sizeof(int) + dirSize + sizeof cursor + sizeof(int));

	*pack_size = 0;
	memcpy(leer_serial + *pack_size, &head, HEAD_SIZE);
	*pack_size += HEAD_SIZE;

	*pack_size += sizeof(int);

	memcpy(leer_serial + *pack_size, &dirSize, sizeof(int)),
	*pack_size += sizeof(int);
	memcpy(leer_serial + *pack_size, path, dirSize);
	*pack_size += dirSize;
	memcpy(leer_serial + *pack_size, &cursor, sizeof(int));
	*pack_size += sizeof(int);
	memcpy(leer_serial + *pack_size, &size, sizeof(int));
	*pack_size += sizeof(int);

	memcpy(leer_serial + HEAD_SIZE, pack_size, sizeof(int));

	return leer_serial;
}

tPackRecvRW *deserializeLeerFS2(char *leer_serial){

	int off;
	tPackRecvRW *prw = malloc(sizeof *prw);

	off = 0;
	memcpy(&prw->dirSize, leer_serial + off, sizeof(int));
	off += sizeof(int);
	prw->direccion = malloc(prw->dirSize);
	memcpy(prw->direccion, leer_serial + off, prw->dirSize);
	off += prw->dirSize;
	memcpy(&prw->cursor, leer_serial + off, sizeof(int));
	off += sizeof(int);
	memcpy(&prw->size, leer_serial + off, sizeof(int));
	off += sizeof(int);

	return prw;
}

char * serializeLeerFS(tPackHeader head, t_direccion_archivo path, void * info,t_valor_variable tamanio,t_banderas flag ,int * pack_size){
	int dirSize = strlen(path)+1;
	char * leer_fs_serial = malloc(HEAD_SIZE + sizeof(int)+ sizeof(int) + dirSize + sizeof tamanio + tamanio + sizeof(flag));

	*pack_size = 0;
	memcpy(leer_fs_serial + *pack_size, &head, HEAD_SIZE);
	*pack_size += HEAD_SIZE;
	*pack_size += sizeof(int);
	memcpy(leer_fs_serial + *pack_size,&dirSize,sizeof(int)),
	*pack_size += sizeof(int);

	memcpy(leer_fs_serial + *pack_size, path, dirSize);
	*pack_size += dirSize;

	memcpy(leer_fs_serial + *pack_size,&tamanio,sizeof(tamanio));
	*pack_size += sizeof(tamanio);

	memcpy(leer_fs_serial + *pack_size, info, tamanio);
	*pack_size += tamanio;

	memcpy(leer_fs_serial + *pack_size,&flag,sizeof(flag)),
	*pack_size += sizeof(int);

	memcpy(leer_fs_serial + HEAD_SIZE,pack_size,sizeof(int));

	return leer_fs_serial;
}

tPackRecibirRW * deserializeLeerFS(char * recibir_serial){
	tPackRecibirRW * aux = malloc(sizeof(*aux));
	int off = 0;

	memcpy(&aux->dirSize,off + recibir_serial,sizeof(int));
	off += sizeof(int);
	aux->direccion = malloc(aux->dirSize);
	memcpy(aux->direccion,off + recibir_serial,aux->dirSize);
	off += aux->dirSize;
	memcpy(&aux->tamanio,off + recibir_serial,sizeof(int));
	off += sizeof(int);
	aux->info = malloc(aux->tamanio);
	memcpy(aux->info,off + recibir_serial,aux->tamanio);
	off += aux->tamanio;
	memcpy(&aux->flag,off + recibir_serial,sizeof(aux->flag));
	off += sizeof(aux->flag);

	return aux;

}
/*
 * FUNCIONES EXTRA...
 */

/* Retorna el peso en bytes de todas las listas y variables sumadas del stack
 */
int sumarPesosStack(t_list *stack){

	int i, sum;
	indiceStack *temp;

	for (i = sum = 0; i < list_size(stack); ++i){
		temp = list_get(stack, i);
		sum += list_size(temp->args) * sizeof (posicionMemoria) + list_size(temp->vars) * sizeof (posicionMemoriaId)
				+ sizeof temp->retPos + sizeof temp->retVar;
	}

	return sum;
}

void informarResultado(int sock, tPackHeader head){
	char *buffer;
	int pack_size, stat;
	pack_size = 0;
	if ((buffer = serializeHeader(head, &pack_size)) == NULL){
		puts("No se pudo serializar el Header de Fallo");
		return;
	}
	if ((stat = send(sock, buffer, pack_size, 0)) == -1){
		perror("Error en envio del Informe de Fallo. error");
		return;
	}
	if (stat != pack_size){
		puts("No se pudo enviar el paquete completo");
		return;
	}
	//printf("Se enviaron %d bytes al socket %d\n", stat, sock);
	free(buffer);
}
