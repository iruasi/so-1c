#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <math.h>

#include <parser/metadata_program.h>
#include <commons/collections/list.h>

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

	if ((stat = send(fd_mem, pack_src_serial, packageSize, 0)) == -1){
		perror("Error en el envio de codigo fuente. error");
		return FALLO_SEND;
	}

	free(pack_src_serial);
	return 0;
}


tPCB *nuevoPCB(tPackSrcCode *src_code, int cant_pags, int sock_hilo){

	t_metadata_program *meta = metadata_desde_literal(src_code->sourceCode);
	t_size indiceCod_size = meta->instrucciones_size * 2 * sizeof(int);

	tPCB *nuevoPCB              = malloc(sizeof *nuevoPCB);
	nuevoPCB->indiceDeCodigo    = malloc(indiceCod_size);
	nuevoPCB->indiceDeStack     = list_create();
	nuevoPCB->indiceDeEtiquetas = malloc(meta->etiquetas_size);

	nuevoPCB->id = globalPID;
	globalPID++;
	nuevoPCB->pc = meta->instruccion_inicio;
	nuevoPCB->paginasDeCodigo = cant_pags;
	nuevoPCB->etiquetas_size         = meta->etiquetas_size;
	nuevoPCB->cantidad_etiquetas     = meta->cantidad_de_etiquetas;
	nuevoPCB->estado_proc = 0;
	nuevoPCB->proxima_rafaga = 0;
	nuevoPCB->cantidad_instrucciones = meta->instrucciones_size;
	nuevoPCB->estado_proc    = 0;
	nuevoPCB->contextoActual = 0;
	nuevoPCB->exitCode       = 0;

	memcpy(nuevoPCB->indiceDeCodigo, meta->instrucciones_serializado, indiceCod_size);

	if (nuevoPCB->cantidad_etiquetas)
		memcpy(nuevoPCB->indiceDeEtiquetas, meta->etiquetas, nuevoPCB->etiquetas_size);

/*	dataHiloProg hp;
	hp.pid = globalPID;
	hp.sock = sock_hilo;
	list_add(listaProgramas, &hp);
	//almacenar(nuevoPCB->id, meta);
*/
	return nuevoPCB;
}


void cpu_manejador(void *sockYmsj){

	t_cpuInfo *sm = (t_cpuInfo *) sockYmsj;
	tPackHeader head = {.tipo_de_proceso = KER, .tipo_de_mensaje = sm->msj};
	tMensaje rta;
	bool found;
	char *buffer;
	char *var = NULL;
	int stat, pack_size;
	tPackBytes *var_name;
	t_valor_variable val;


	do {
	printf("proc: %d  \t msj: %d\n", head.tipo_de_proceso, head.tipo_de_mensaje);

	switch(head.tipo_de_mensaje){
	case S_WAIT:
		puts("Signal wait a semaforo");
		//pasarABlock();
		break;
	case S_SIGNAL:
		//planificadorPasarDeBlock();
		puts("Signal continuar a semaforo");
		break;

	case SET_GLOBAL:
		puts("Se reasigna una variable global");

		if ((buffer = recvGeneric(sm->cpu.fd_cpu)) == NULL){
			puts("Fallo recepcion generica");
			break;
		}

		tPackValComp *val_comp;
		if ((val_comp = deserializeValorYVariable(buffer)) == NULL){
			puts("No se pudo deserializar Valor y Variable");
			// todo: abortar programa?
			break;
		}

		if ((stat = setGlobal(val_comp)) != 0){
			puts("No se pudo asignar la variable global");
			// todo: abortar programa?
			break;
		}

		freeAndNULL((void **) &buffer);
		freeAndNULL((void **) &val_comp);
		break;

	case GET_GLOBAL:
		puts("Se pide el valor de una variable global");

		if ((buffer = recvGeneric(sm->cpu.fd_cpu)) == NULL){
			puts("Fallo recepcion generica");
			break;
		}

		var_name = deserializeBytes(buffer);
		freeAndNULL((void **) &buffer);

		var = realloc(var, var_name->bytelen);
		memcpy(var, var_name->bytes, var_name->bytelen);

		val = getGlobal(var, &found);
		rta = (found)? GET_GLOBAL : GLOBAL_NOT_FOUND;
		head.tipo_de_mensaje = rta;

		if ((buffer = serializeValorYVariable(head, val, var, &pack_size)) == NULL){
			puts("No se pudo serializar Valor Y Variable");
			return;
		}

		if ((stat = send(sm->cpu.fd_cpu, buffer, pack_size, 0)) == -1){
			perror("Fallo send de Valor y Variable. error");
			return;
		}

		freeAndNULL((void **) &buffer);
		freeAndNULL((void**) &var_name->bytes); freeAndNULL((void **) &var_name);
		break;

	case LIBERAR:
		puts("Funcion liberar");
		break;
	case ABRIR:
		break;
	case BORRAR:
		break;
	case CERRAR:
		break;
	case MOVERCURSOR:
		break;
	case ESCRIBIR:

		buffer = recvGeneric(sm->cpu.fd_cpu);
		tPackEscribir *escr = deserializeEscribir(buffer);

		printf("Se escriben en fd %d, la info %s\n", escr->fd, (char*) escr->info);
		free(escr->info); free(escr);
		break;

	case LEER:
		break;
	case RESERVAR:
		break;
	case HSHAKE:
		puts("Es solo un handshake");
		break;

	case(FIN_PROCESO): case(ABORTO_PROCESO): case(RECURSO_NO_DISPONIBLE): //COLA EXIT
		cpu_handler_planificador(sm);
	break;

	default:
		puts("Funcion no reconocida!");
		break;

	}} while((stat = recv(sm->cpu.fd_cpu, &head, sizeof head, 0)) > 0);

}
