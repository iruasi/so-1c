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

int contestarMemoriaKernel(int marco_size, int marcos, int sock_ker){

	int stat, pack_size;
	char *hs_serial;

	tHShakeMemAKer *h_shake = malloc(sizeof *h_shake);
	h_shake->head.tipo_de_proceso = MEM;
	h_shake->head.tipo_de_mensaje = MEMINFO;
	h_shake->marco_size = marco_size;
	h_shake->marcos = marcos;

	if ((hs_serial = serializeMemAKer(h_shake, &pack_size)) == NULL){
		puts("Fallo la serializacion de handshake Memoria a Kernel");
		return FALLO_SERIALIZAC;
	}

	if((stat = send(sock_ker, hs_serial, pack_size, 0)) == -1)
		perror("Error de envio informacion Memoria a Kernel. error");

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
		puts("No se pudo serializar el handshake de Memoria a CPU");
		return FALLO_SERIALIZAC;
	}

	if((stat = send(sock, hs_serial, pack_size, 0)) == -1)
		perror("Error de envio informacion Memoria a CPU. error");

	free(hs_serial);
	return stat;
}

int contestarMemoriaCPU(int marco_size, int sock_cpu){

	int stat, pack_size;
	char *hs_serial;

	tHShakeProcAProc h_shake;
	h_shake.head.tipo_de_proceso = MEM;
	h_shake.head.tipo_de_mensaje = MEMINFO;
	h_shake.val = marco_size;

	pack_size = 0;
	if ((hs_serial = serializeProcAProc(&h_shake, &pack_size)) == NULL){
		puts("No se pudo serializar el handshake de Memoria a CPU");
		return FALLO_SERIALIZAC;
	}

	if((stat = send(sock_cpu, hs_serial, pack_size, 0)) == -1)
		perror("Error de envio informacion Memoria a CPU. error");

	free(hs_serial);
	return stat;
}

int recibirInfoKerMem(int sock_mem, int *frames, int *frame_size){

	int stat;
	char *info_serial;
	tPackHeader head;

	if ((stat = recv(sock_mem, &head, HEAD_SIZE, 0)) == -1){
		perror("Fallo recepcion de info de Memoria. error");
		return FALLO_RECV;
	}

	if (head.tipo_de_proceso != MEM || head.tipo_de_mensaje != MEMINFO){
		printf("El paquete recibido no era el esperado! Proceso: %d, Mensaje: %d\n",
				head.tipo_de_proceso, head.tipo_de_mensaje);
		return FALLO_GRAL;
	}

	if ((info_serial = recvGeneric(sock_mem)) == NULL){
		puts("Fallo la creacion de info serializada desde Memoria");
		return FALLO_GRAL;
	}

	memcpy(frames, info_serial, sizeof(int));
	memcpy(frame_size, info_serial + sizeof(int), sizeof(int));

	free(info_serial);
	return 0;
}

int recibirInfoCPUMem(int sock_mem, int *frame_size){

	int stat;
	char *info_serial;
	tPackHeader head;

	if ((stat = recv(sock_mem, &head, HEAD_SIZE, 0)) == -1){
		perror("Fallo recepcion de info de Memoria. error");
		return FALLO_RECV;
	}

	if (head.tipo_de_proceso != MEM || head.tipo_de_mensaje != MEMINFO){
		printf("El paquete recibido no era el esperado! Proceso: %d, Mensaje: %d\n",
				head.tipo_de_proceso, head.tipo_de_mensaje);
		return FALLO_GRAL;
	}

	if ((info_serial = recvGeneric(sock_mem)) == NULL){
		puts("Fallo la creacion de info serializada desde Memoria");
		return FALLO_GRAL;
	}

	memcpy(frame_size, info_serial, sizeof(int));
	free(info_serial);
	return 0;
}

int recibirInfoCPUKernel(int sock_kern, int *q_sleep){

	int stat;
	char *info_serial;
	tPackHeader head;

	if ((stat = recv(sock_kern, &head, HEAD_SIZE, 0)) == -1){
		perror("Fallo recepcion de info de Memoria. error");
		return FALLO_RECV;
	}

	if (head.tipo_de_proceso != KER || head.tipo_de_mensaje != KERINFO){
		printf("El paquete recibido no era el esperado! Proceso: %d, Mensaje: %d\n",
				head.tipo_de_proceso, head.tipo_de_mensaje);
		return FALLO_GRAL;
	}

	if ((info_serial = recvGeneric(sock_kern)) == NULL){
		puts("Fallo la creacion de info serializada desde Memoria");
		return FALLO_GRAL;
	}

	memcpy(q_sleep, info_serial, sizeof(int));
	free(info_serial);
	return 0;
}


/****** Definiciones de [De]Serializaciones Handshakes especiales ******/

char *serializeMemAKer(tHShakeMemAKer *h_shake, int *pack_size){

	char *hs_serial;

	if ((hs_serial = malloc(sizeof *h_shake + sizeof(int))) == NULL){
		printf("No se pudieron mallocar %d bytes para el handshake de Memoria a Kernel\n", sizeof *h_shake);
		return NULL;
	}

	*pack_size = 0;
	memcpy(hs_serial + *pack_size, &h_shake->head, HEAD_SIZE);
	*pack_size += HEAD_SIZE;

	*pack_size += sizeof(int);

	memcpy(hs_serial + *pack_size, &h_shake->marcos, sizeof(int));
	*pack_size += sizeof(int);
	memcpy(hs_serial + *pack_size, &h_shake->marco_size, sizeof(int));
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

char *recvGeneric(int sock_in){
	puts("Se recibe el paquete serializado..");

	int stat, pack_size;
	char *p_serial;

	if ((stat = recv(sock_in, &pack_size, sizeof(int), 0)) == -1){
		perror("Fallo de recv. error");
		return NULL;

	} else if (stat == 0){
		printf("El proceso del socket %d se desconecto. No se pudo completar recvGenerico\n", sock_in);
		return NULL;
	}

	printf("Paquete de size: %d\n", pack_size);

	if ((p_serial = malloc(pack_size)) == NULL){
		printf("No se pudieron mallocar %d bytes para paquete generico\n", pack_size);
		return NULL;
	}

	if ((stat = recv(sock_in, p_serial, pack_size, 0)) == -1){
		perror("Fallo de recv. error");
		return NULL;

	} else if (stat == 0){
		printf("El proceso del socket %d se desconecto. No se pudo completar recvGenerico\n", sock_in);
		return NULL;
	}

	return p_serial;
}

/****** Definiciones de [De]Serializaciones Regulares ******/

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

	// serializamos indice de codigo
	memcpy(pcb_serial + off, pcb->indiceDeCodigo, indiceCod_size);
	off += indiceCod_size;

	// serializamos indice de stack
	*pack_size = 0;
	char *stack_serial = serializarStack(pcb, indiceStack_size, pack_size);
	memcpy(pcb_serial + off, stack_serial, *pack_size);
	off += *pack_size;

	// serializamos indice de etiquetas
	if (pcb->cantidad_etiquetas){
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
	puts("Deserializamos PCB");

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

	indiceCod_size = pcb->cantidad_instrucciones * 2 * sizeof(int);
	if ((pcb->indiceDeCodigo = malloc(indiceCod_size)) == NULL){
		fprintf(stderr, "Fallo malloc\n");
		return NULL;
	}

	memcpy(pcb->indiceDeCodigo, pcb_serial + offset, indiceCod_size);
	offset += indiceCod_size;

	deserializarStack(pcb, pcb_serial, &offset);

	pcb->indiceDeEtiquetas = NULL;
	if (pcb->cantidad_etiquetas){ // si hay etiquetas, las memcpy'amos
		pcb->indiceDeEtiquetas = malloc(pcb->etiquetas_size);
		memcpy(pcb->indiceDeEtiquetas, pcb_serial + offset, pcb->etiquetas_size);
		offset += pcb->cantidad_etiquetas;
	}

	return pcb;
}

void deserializarStack(tPCB *pcb, char *pcb_serial, int *offset){
	puts("Deserializamos stack..");

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

	int payload_size;
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

	payload_size = *pack_size - (HEAD_SIZE + sizeof(int));
	memcpy(byterq_serial + HEAD_SIZE, &payload_size, sizeof(int));

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
	puts("deserializamos bytes almacenamiento");

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

tPackSrcCode *recvbytes(int sock_in){

	int stat;
	tPackSrcCode *src_pack;
	if ((src_pack = malloc(sizeof *src_pack)) == NULL){
		perror("No se pudo mallocar espacio para el paquete src_pack. error");
		return NULL;
	}

	// recibimos el valor de size que va a tener el codigo fuente
	if ((stat = recv(sock_in, &src_pack->bytelen, sizeof (unsigned long), 0)) <= 0){
		perror("El socket cerro la conexion o hubo fallo de recepcion. error");
		errno = FALLO_RECV;
		return NULL;
	}

	// hacemos espacio para el codigo fuente
	if ((src_pack->bytes = malloc(src_pack->bytelen)) == NULL){
		perror("No se pudo mallocar espacio para el src_pack->bytes. error");
		return NULL;
	}

	if ((stat = recv(sock_in, src_pack->bytes, src_pack->bytelen, 0)) <= 0){
		perror("El socket cerro la conexion o hubo fallo de recepcion. error");
		errno = FALLO_RECV;
		return NULL;
	}

	return src_pack;
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
	freeAndNULL((void **) &bufferCode);
	return src_pack;
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

char *serializeAbrir(t_direccion_archivo direccion, t_banderas flags, int *pack_size){

	tPackHeader head = {.tipo_de_proceso = CPU, .tipo_de_mensaje = ABRIR};
	int dirSize = strlen(direccion);

	char *abrir_serial;
	abrir_serial = malloc(HEAD_SIZE + sizeof(int) + sizeof(int) + dirSize + sizeof flags);

	*pack_size = 0;
	memcpy(abrir_serial + *pack_size, &head, HEAD_SIZE);
	*pack_size += HEAD_SIZE;

	*pack_size += sizeof(int);

	memcpy(abrir_serial + *pack_size, &dirSize, sizeof(int));
	*pack_size += sizeof(int);
	memcpy(abrir_serial + *pack_size, direccion, dirSize);
	*pack_size += dirSize;
	memcpy(abrir_serial + *pack_size, &flags, sizeof flags);
	*pack_size += sizeof flags;

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

tPackEscribir *deserializeEscribir(char *escr_serial){

	tPackEscribir *escr;
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


char *serializeLeer(t_descriptor_archivo descriptor_archivo, t_puntero informacion, t_valor_variable tamanio, int *pack_size){

	tPackHeader head = {.tipo_de_proceso = CPU, .tipo_de_mensaje = LEER};

	char *leer_serial;
	leer_serial = malloc(HEAD_SIZE + sizeof(int) + sizeof descriptor_archivo + sizeof tamanio + tamanio);

	*pack_size = 0;
	memcpy(leer_serial + *pack_size, &head, HEAD_SIZE);
	*pack_size += HEAD_SIZE;

	*pack_size += sizeof(int);

	memcpy(leer_serial + *pack_size, &descriptor_archivo, sizeof descriptor_archivo);
	*pack_size += sizeof descriptor_archivo;
	memcpy(leer_serial + *pack_size, &tamanio, sizeof tamanio);
	*pack_size += sizeof tamanio;
	memcpy(leer_serial + *pack_size, &informacion, tamanio);
	*pack_size += tamanio;

	memcpy(leer_serial + HEAD_SIZE, pack_size, sizeof(int));

	return leer_serial;
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



/*
 * FUNCIONES EXTRA... //todo: deberia ir en compartidas, no?
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

