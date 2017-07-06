#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <semaphore.h>
#include <pthread.h>
#include <math.h>

#include <parser/metadata_program.h>
#include <commons/collections/list.h>

#include "kernelConfigurators.h"
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

extern sem_t hayProg;
int globalPID;

t_list *gl_Programas; // va a almacenar relaciones entre Programas y Codigo Fuente
t_list *listaDeCpu;

extern tKernel *kernel;
extern t_valor_variable *shared_vals;

/* Este procedimiento inicializa las variables y listas globales.
 */
void setupVariablesGlobales(void){

	gl_Programas = list_create();

}

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

tPCB *crearPCBInicial(void){

	tPCB *pcb;
	if ((pcb = malloc(sizeof *pcb)) == NULL){
		printf("Fallo mallocar %d bytes para pcbInicial\n", sizeof *pcb);
		return NULL;
	}
	pcb->indiceDeCodigo    = NULL;
	pcb->indiceDeStack     = list_create();
	pcb->indiceDeEtiquetas = NULL;

	pcb->id = globalPID;
	globalPID++;
	pcb->proxima_rafaga = 0;
	pcb->estado_proc    = 0;
	pcb->contextoActual = 0;
	pcb->exitCode       = 0;

	return pcb;
}

tPCB *nuevoPCB(tPackSrcCode *src_code, int cant_pags, t_RelCC *prog){

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
	nuevoPCB->proxima_rafaga = 0;
	nuevoPCB->cantidad_instrucciones = meta->instrucciones_size;
	nuevoPCB->estado_proc    = 0;
	nuevoPCB->contextoActual = 0;
	nuevoPCB->exitCode       = 0;

	memcpy(nuevoPCB->indiceDeCodigo, meta->instrucciones_serializado, indiceCod_size);

	if (nuevoPCB->cantidad_etiquetas)
		memcpy(nuevoPCB->indiceDeEtiquetas, meta->etiquetas, nuevoPCB->etiquetas_size);

	prog->con->pid = nuevoPCB->id; // asociamos este PCB al programa
/*	dataHiloProg hp;
	hp.pid = globalPID;
	hp.sock = sock_hilo;
	list_add(listaProgramas, &hp);
	//almacenar(nuevoPCB->id, meta);
*/
	return nuevoPCB;
}


void cpu_manejador(void *infoCPU){

	t_RelCC *cpu_i = (t_RelCC *) infoCPU;
	printf("cpu_manejador socket %d\n", cpu_i->cpu.fd_cpu);

	pthread_mutex_lock(&mux_listaDeCPU);
	list_add(listaDeCpu, cpu_i); pthread_mutex_unlock(&mux_listaDeCPU);

	tPackHeader head = {.tipo_de_proceso = CPU, .tipo_de_mensaje = THREAD_INIT};
	tMensaje rta;
	bool found;
	char *buffer;
	char *var = NULL;
	int stat, pack_size;

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

		if ((buffer = recvGeneric(cpu_i->cpu.fd_cpu)) == NULL){
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
		t_valor_variable val;
		tPackBytes *var_name;

		if ((buffer = recvGeneric(cpu_i->cpu.fd_cpu)) == NULL){
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

		if ((stat = send(cpu_i->cpu.fd_cpu, buffer, pack_size, 0)) == -1){
			perror("Fallo send de Valor y Variable. error");
			return;
		}

		freeAndNULL((void **) &buffer);
		freeAndNULL((void **) &var_name->bytes); freeAndNULL((void **) &var_name);
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

		buffer = recvGeneric(cpu_i->cpu.fd_cpu);
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
		cpu_i->msj = head.tipo_de_mensaje;
		cpu_handler_planificador(cpu_i);
	break;

	default:
		puts("Funcion no reconocida!");
		break;

	}} while((stat = recv(cpu_i->cpu.fd_cpu, &head, sizeof head, 0)) > 0);

}


void cons_manejador(void *conInfo){
	t_RelCC *con_i = (t_RelCC*) conInfo;
	printf("cons_manejador socket %d\n", con_i->con->fd_con);

	int stat;
	tPackHeader head = {.tipo_de_proceso = CPU, .tipo_de_mensaje = THREAD_INIT};

	do {
	switch(head.tipo_de_mensaje){
	case SRC_CODE:
		puts("Consola quiere iniciar un programa");

		tPackSrcCode *entradaPrograma;

		//recvGeneric(con_i->con->fd_con);

		entradaPrograma = recibir_paqueteSrc(con_i->con->fd_con); // Aca voy a recibir el tPackSrcCode
		tPCB *new_pcb = crearPCBInicial();
		con_i->con->pid = new_pcb->id;
		asociarSrcAProg(con_i, entradaPrograma);

		printf("El size del paquete %d\n", strlen(entradaPrograma->sourceCode) + 1);

		encolarEnNew(new_pcb);
		sem_post(&hayProg);
		//tPCB *new_pcb = nuevoPCB(entradaPrograma, cant_pag, con_i);

		//encolarEnNewPrograma(new_pcb, con_i->con->fd_con);

		puts("Fin case SRC_CODE.");
		break;

	case THREAD_INIT:
		puts("Se inicia thread en handler de Consola");
		puts("Fin case THREAD_INIT.");
		break;

	case HSHAKE:
		puts("Es solo un handshake");
		break;

	default:
		break;
	}
	}while ((stat = recv(con_i->con->fd_con, &head, HEAD_SIZE, 0)) > 0);


}

void asociarSrcAProg(t_RelCC *con_i, tPackSrcCode *src){
	puts("Asociar Programa y Codigo Fuente");

	t_RelPF *pf;
	if ((pf = malloc(sizeof *pf)) == NULL){
		printf("No se pudieron mallocar %d bytes para RelPF\n", sizeof *pf);
		return;
	}

	pf->prog = con_i;
	pf->src  = src;
	list_add(gl_Programas, pf);
}


tPackSrcCode *recibir_paqueteSrc(int fd){ //Esta funcion tiene potencial para recibir otro tipos de paquetes

	int paqueteRecibido;
	int *tamanioMensaje = malloc(sizeof (int));

	paqueteRecibido = cantidadTotalDeBytesRecibidos(fd, (char *) tamanioMensaje, sizeof(int));
	if(paqueteRecibido <= 0 ) return NULL;

	void *mensaje = malloc(*tamanioMensaje);
	paqueteRecibido = cantidadTotalDeBytesRecibidos(fd, mensaje, *tamanioMensaje);
	if(paqueteRecibido <= 0) return NULL;

	tPackSrcCode *pack_src = malloc(sizeof *pack_src);
	pack_src->sourceLen = paqueteRecibido;
	pack_src->sourceCode = malloc(pack_src->sourceLen);
	memcpy(pack_src->sourceCode, mensaje, paqueteRecibido);

	//tPackSrcCode *buffer = deserializeSrcCode(fd);

	free(tamanioMensaje);tamanioMensaje = NULL;
	free(mensaje);mensaje = NULL;

	return pack_src;

}


int setGlobal(tPackValComp *val_comp){

	int i;
	int nlen = strlen(val_comp->nom) + 2; // espacio para el ! y el '\0'
	char *aux = NULL;
	for (i = 0; i < kernel->shared_quant; ++i){
		aux = realloc(aux, nlen); aux[0] = '!'; memcpy(aux + 1, val_comp->nom, nlen); aux[nlen] = '\0';

		if (strcmp(kernel->shared_vars[i], aux) == 0){
			shared_vals[i] = val_comp->val;
			free(aux);
			return 0;
		}
	}
	free(aux);
	return GLOBAL_NOT_FOUND;
}

t_valor_variable getGlobal(t_nombre_variable *var, bool* found){

	int i;
	int nlen = strlen(var) + 2; // espacio para el ! y el '\0'
	char *aux = NULL;
	*found = true;
	for (i = 0; i < kernel->shared_quant; ++i){
		aux = realloc(aux, nlen); aux[0] = '!'; memcpy(aux + 1, var, nlen); aux[nlen] = '\0';

		if (strcmp(kernel->shared_vars[i], aux) == 0){
			free(aux);
			return shared_vals[i];
		}
	}
	free(aux);
	*found = false;
	return GLOBAL_NOT_FOUND;
}
