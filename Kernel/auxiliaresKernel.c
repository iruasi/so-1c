#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <semaphore.h>
#include <pthread.h>
#include <math.h>

#include <commons/collections/queue.h>
#include <parser/metadata_program.h>
#include <commons/collections/list.h>

#include "kernelConfigurators.h"
#include "auxiliaresKernel.h"
#include "planificador.h"
#include "funcionesSyscalls.h"
#include "capaMemoria.h"

#include <funcionesCompartidas/funcionesCompartidas.h>
#include <funcionesPaquetes/funcionesPaquetes.h>
#include <tiposRecursos/tiposErrores.h>
#include <tiposRecursos/tiposPaquetes.h>
#include <tiposRecursos/misc/pcb.h>
#define MAXOPCION 200

#ifndef HEAD_SIZE
#define HEAD_SIZE 8
#endif

//extern t_dictionary *heapDict;
extern sem_t sem_heapDict;
extern sem_t sem_bytes;
extern sem_t sem_end_exec;

extern sem_t hayCPUs;
extern sem_t eventoPlani;
int globalPID;

t_list *gl_Programas; // va a almacenar relaciones entre Programas y Codigo Fuente
t_list *listaDeCpu;

extern t_queue *New, *Exit, *Block,*Ready;
extern t_list	*Exec,*listaProgramas;
extern tKernel *kernel;
extern int grado_mult;

/* Este procedimiento inicializa las variables y listas globales.
 */
void setupVariablesGlobales(void){

	gl_Programas = list_create();

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

	t_metadata_program *meta = metadata_desde_literal(src_code->bytes);
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

	tPackHeader head = {.tipo_de_proceso = CPU, .tipo_de_mensaje = THREAD_INIT};
	tMensaje rta;
	bool found;
	char *buffer;
	char *var;
	int stat, pack_size;
	tPackBytes *sem_bytes;
	tPackVal *alloc;
	t_puntero ptr;

	do {
	printf("(CPU) proc: %d  \t msj: %d\n", head.tipo_de_proceso, head.tipo_de_mensaje);

	switch(head.tipo_de_mensaje){
	case S_WAIT:
		puts("Signal wait a semaforo");
		buffer = recvGeneric(cpu_i->cpu.fd_cpu);
		sem_bytes = deserializeBytes(buffer);
		waitSyscall(sem_bytes->bytes, cpu_i->cpu.pid);

		freeAndNULL((void **) &buffer);
		freeAndNULL((void **) &sem_bytes);
		break;

	case S_SIGNAL:
		puts("Signal continuar a semaforo");
		buffer = recvGeneric(cpu_i->cpu.fd_cpu);
		sem_bytes = deserializeBytes(buffer);
		signalSyscall(sem_bytes->bytes, cpu_i->cpu.pid);

		freeAndNULL((void **) &buffer);
		freeAndNULL((void **) &sem_bytes);
		break;

	case SET_GLOBAL:
		puts("Se reasigna una variable global");

		if ((buffer = recvGeneric(cpu_i->cpu.fd_cpu)) == NULL){
			puts("Fallo recepcion generica");
			break;
		}

		tPackValComp *val_comp;
		if ((val_comp = deserializeValorYVariable(buffer)) == NULL){
			head.tipo_de_proceso = KER; head.tipo_de_mensaje = FALLO_DESERIALIZAC;
			//informarFallo(cpu_i->cpu.fd_cpu, head);
			//finalizarProceso(pid) // todo: funcion finalizar proceso
			break;
		}

		if ((stat = setGlobalSyscall(val_comp)) != 0){
			head.tipo_de_proceso = KER; head.tipo_de_mensaje = stat;
			//informarFallo(cpu_i->cpu.fd_cpu, head);
			//finalizarProceso(pid) // todo: funcion finalizar proceso
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

		var = malloc(var_name->bytelen);
		memcpy(var, var_name->bytes, var_name->bytelen);

		val = getGlobalSyscall(var, &found);
		rta = (found)? GET_GLOBAL : GLOBAL_NOT_FOUND;
		head.tipo_de_proceso = KER; head.tipo_de_mensaje = rta;

		if ((buffer = serializeValorYVariable(head, val, var, &pack_size)) == NULL){
			puts("No se pudo serializar Valor Y Variable");
			return;
		}

		if ((stat = send(cpu_i->cpu.fd_cpu, buffer, pack_size, 0)) == -1){
			perror("Fallo send de Valor y Variable. error");
			return;
		}

		freeAndNULL((void **) &buffer);
		freeAndNULL((void **) &var);
		freeAndNULL((void **) &var_name->bytes); freeAndNULL((void **) &var_name);
		break;

	case RESERVAR:
		puts("Funcion reservar");

		buffer = recvGeneric(cpu_i->cpu.fd_cpu);
		alloc = deserializeVal(buffer);
		freeAndNULL((void **) &buffer);

		if ((ptr = reservar(cpu_i->cpu.pid, alloc->val)) == 0){
			head.tipo_de_proceso = KER; head.tipo_de_mensaje = FALLO_HEAP;
			informarFallo(cpu_i->cpu.fd_cpu, head);
			//finalizarProceso(pid) // todo: funcion finalizar proceso
			break;
		}

		alloc->head.tipo_de_proceso = KER; alloc->head.tipo_de_mensaje = RESERVAR;
		alloc->val = ptr;
		pack_size = 0;
		buffer = serializeVal(alloc, &pack_size);

		if ((stat = send(cpu_i->cpu.fd_cpu, buffer, pack_size, 0)) == -1){
			perror("Fallo send de puntero alojado a CPU. error");
			break;
		}

		freeAndNULL((void **) &buffer);
		freeAndNULL((void **) &alloc);
		break;

	case LIBERAR:
		puts("Funcion liberar");
		buffer = recvGeneric(cpu_i->cpu.fd_cpu);
		alloc = deserializeVal(buffer);
		freeAndNULL((void **) &buffer);

		liberar(cpu_i->cpu.pid, alloc->val);

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
		freeAndNULL((void **) &buffer);
		break;

	case LEER:
		break;

	case(FIN_PROCESO): case(ABORTO_PROCESO): case(RECURSO_NO_DISPONIBLE): case(PCB_PREEMPT): //COLA EXIT
		cpu_i->msj = head.tipo_de_mensaje;
		cpu_handler_planificador(cpu_i);
	break;

	case HSHAKE:
		puts("Se recibe handshake de CPU");

		head.tipo_de_proceso = KER; head.tipo_de_mensaje = KERINFO;
		if ((stat = contestarProcAProc(head, kernel->quantum_sleep, cpu_i->cpu.fd_cpu)) < 0){
			puts("No se pudo informar el quantum_sleep a CPU.");
			return;
		}

		pthread_mutex_lock(&mux_listaDeCPU);
		list_add(listaDeCpu, cpu_i);
		pthread_mutex_unlock(&mux_listaDeCPU);
		//sem_post(&hayCPUs);
		sem_post(&eventoPlani);
		puts("Fin case HSHAKE.");
		break;

	case THREAD_INIT:
		puts("Se inicia thread en handler de CPU");
		puts("Fin case THREAD_INIT.");
		break;

	default:
		puts("Funcion no reconocida!");
		break;

	}} while((stat = recv(cpu_i->cpu.fd_cpu, &head, sizeof head, 0)) > 0);

	if (stat == -1){
		perror("Error de recepcion de CPU. error");
		return;
	}

	puts("CPU se desconecto, la sacamos de la listaDeCpu..");
	pthread_mutex_lock(&mux_listaDeCPU);
	cpu_i = list_remove(listaDeCpu, getCPUPosByFD(cpu_i->cpu.fd_cpu, listaDeCpu));
	pthread_mutex_unlock(&mux_listaDeCPU);
	liberarCC(cpu_i);
}

void liberarCC(t_RelCC *cc){
	free(cc->con);
	free(cc);
}

int getCPUPosByFD(int fd, t_list *list){

	int i;
	t_RelCC *cc;
	for (i = 0; i < list_size(list); ++i){
		cc = list_get(list, i);
		if (cc->cpu.fd_cpu == fd)
			return i;
	}

	printf("No se encontro el CPU de socket %d en la listaDeCpu", fd);
	return -1;
}

void mem_manejador(void *m_sock){
	int *sock_mem = (int*) m_sock;
	printf("mem_manejador socket %d\n", *sock_mem);

	int stat;
	tPackHeader head = {.tipo_de_proceso = MEM, .tipo_de_mensaje = THREAD_INIT};

	do {
	switch(head.tipo_de_mensaje){
	printf("(MEM) proc: %d  \t msj: %d\n", head.tipo_de_proceso, head.tipo_de_mensaje);

	case ASIGN_SUCCS : case FALLO_ASIGN:
		puts("Se recibe respuesta de asignacion de paginas para algun proceso");
		sem_post(&sem_heapDict);
		sem_wait(&sem_end_exec);
		puts("Fin case ASIGN_SUCCESS_OR_FAIL");
		break;

	case BYTES:
		puts("Se reciben bytes desde Memoria");
		sem_post(&sem_bytes);
		sem_wait(&sem_end_exec);
		puts("Fin case BYTES");
		break;

	case THREAD_INIT:
		puts("Se inicia thread en handler de Memoria");
		puts("Fin case THREAD_INIT");
		break;

	default:
		puts("Se recibe un mensaje de Memoria no considerado");
		break;

	}} while ((stat = recv(*sock_mem, &head, HEAD_SIZE, 0)) > 0);


}

void cons_manejador(void *conInfo){
	t_RelCC *con_i = (t_RelCC*) conInfo;
	printf("cons_manejador socket %d\n", con_i->con->fd_con);

	int stat;
	tPackHeader head = {.tipo_de_proceso = CON, .tipo_de_mensaje = THREAD_INIT};
	char *buffer;
	tPackBytes *pbytes;
	tPackSrcCode *entradaPrograma;

	do {
	switch(head.tipo_de_mensaje){
	printf("(CON) proc: %d  \t msj: %d\n", head.tipo_de_proceso, head.tipo_de_mensaje);

	case SRC_CODE:
		puts("Consola quiere iniciar un programa");

		if ((buffer = recvGeneric(con_i->con->fd_con)) == NULL){
			puts("Fallo recepcion de SRC_CODE");
			return;
		}

		if ((entradaPrograma = (tPackSrcCode *) deserializeBytes(buffer)) == NULL){
			puts("Fallo deserializacion de Bytes");
			return;
		}

		tPCB *new_pcb = crearPCBInicial();
		con_i->con->pid = new_pcb->id;
		asociarSrcAProg(con_i, entradaPrograma);

		printf("El size del paquete %d\n", strlen(entradaPrograma->bytes) + 1);

		encolarEnNew(new_pcb);

		freeAndNULL((void **) &pbytes);
		freeAndNULL((void **) &buffer);
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

	}} while ((stat = recv(con_i->con->fd_con, &head, HEAD_SIZE, 0)) > 0);


}

void consolaKernel(void){

	printf("\n \n \nIngrese accion a realizar:\n");
	printf ("1-Para obtener los procesos en todas las colas o 1 especifica: 'procesos <cola>/<todas>'\n");
	printf ("2-Para ver info de un proceso determinado: 'info <PID>'\n");
	printf ("3-Para obtener la tabla global de archivos: 'tabla'\n");
	printf ("4-Para modificar el grado de multiprogramacion: 'nuevoGrado <GRADO>'\n");
	printf ("5-Para finalizar un proceso: 'finalizar <PID>'\n");
	printf ("6-Para detener la planificacion: 'stop'\n");

	int finalizar = 0;
	while(finalizar !=1){
			printf("Seleccione opcion: \n");
			char *opcion=malloc(MAXOPCION);
			fgets(opcion,MAXOPCION,stdin);
			opcion[strlen(opcion) - 1] = '\0';
			if (strncmp(opcion,"procesos",8)==0){
				puts("Opcion procesos");
				char *cola = opcion+9;
				mostrarColaDe(cola);

			}
			if (strncmp(opcion,"info",4)==0){
				puts("Opcion info");
				char *pidInfo=opcion+5;
				int pidElegido = atoi(pidInfo);
				mostrarInfoDe(pidElegido);
				printf("Info de PID: %d  \n",pidElegido);
			}
			if (strncmp(opcion,"tabla",5)==0){
				puts("Opcion tabla");
				mostrarTablaGlobal();
			}
			if (strncmp(opcion,"nuevoGrado",10)==0){
				puts("Opcion nuevoGrado");
				char *grado = opcion+11;
				int nuevoGrado = atoi(grado);
				cambiarGradoMultiprogramacion(nuevoGrado);
			}
			if (strncmp(opcion,"finalizar",9)==0){
				puts("Opcion finalizar");
				char *pidAFinalizar = opcion+9;
				int pidFin = atoi(pidAFinalizar);
				finalizarProceso(pidFin);

			}
			if (strncmp(opcion,"stop",4)==0){
				puts("Opcion stop");
				stopKernel();
			}


		}
}
//TODO: Hay q sincronizar las colas?? Solo las estoy mirando, no tendria pq poner un semaforo, no?
void mostrarColaDe(char* cola){
	printf("%s",cola);
	puts("Mostrar estado de: ");
	if (strncmp(cola,"todas",5)==0){
		puts("Mostrar estado de todas las colas:");
		mostrarColaNew();
		mostrarColaReady();
		mostrarColaExec();
		mostrarColaExit();
		mostrarColaBlock();
	}
	if (strncmp(cola,"new",3)==0){
		puts("Mostrar estado de cola NEW");
		mostrarColaNew();
	}
	if (strncmp(cola,"ready",5)==0){
		puts("Mostrar estado de cola REDADY");
		mostrarColaReady();
	}
	if (strncmp(cola,"exec",4)==0){
		puts("Mostrar estado de cola EXEC");
		mostrarColaExec();
	}
	if (strncmp(cola,"exit",4)==0){
		puts("Mostrar estado de cola EXIT");
		mostrarColaExit();
	}
	if (strncmp(cola,"block",5)==0){
		puts("Mostrar estado de cola BLOCK");
		mostrarColaBlock();
	}
}



void mostrarColaNew(){
	puts("Cola New: ");
	int k=0;
	tPCB * pcbAux;
	for(k=0;k<queue_size(New);k++){
		pcbAux = (tPCB*) queue_get(New,k);
		printf("En la posicion %d, el proceso %d\n",k,pcbAux->id);
	}

}

void mostrarColaReady(){
	puts("Cola Ready: ");
	int k=0;
	tPCB * pcbAux;
	for(k=0;k<queue_size(Ready);k++){
		pcbAux = (tPCB*) queue_get(Ready,k);
		printf("En la posicion %d, el proceso %d\n",k,pcbAux->id);
	}
}

void mostrarColaExec(){
	puts("Cola Exec: ");
	int k=0;
	tPCB * pcbAux;
	for(k=0;k<list_size(Exec);k++){
		pcbAux = (tPCB*) list_get(Exec,k);
		printf("En la posicion %d, el proceso %d\n",k,pcbAux->id);
	}
}

void mostrarColaExit(){
	puts("Cola Exit: ");
	int k=0;
	tPCB * pcbAux;
	for(k=0;k<queue_size(Exit);k++){
		pcbAux = (tPCB*) queue_get(Exit,k);
		printf("En la posicion %d, el proceso %d\n",k,pcbAux->id);
	}
}

void mostrarColaBlock(){
	puts("Cola Block: ");
	int k=0;
	tPCB * pcbAux;
	for(k=0;k<queue_size(Block);k++){
		pcbAux = (tPCB*) queue_get(Block,k);
		printf("En la posicion %d, el proceso %d\n",k,pcbAux->id);
	}
}


void mostrarInfoDe(int pidElegido){
	int j=0;
	printf("Estamos en mostrar info de pid: %d\n",pidElegido);
	tPCB * pcbAuxiliar;
	for(j=0;j<list_size(listaProgramas);j++){
		pcbAuxiliar = (tPCB*) list_get(listaProgramas,j);
		if(pcbAuxiliar->id==pidElegido){
			j=list_size(listaProgramas);
		}
	}

	mostrarCantRafagasEjecutadasDe(pcbAuxiliar);
	mostrarCantPrivilegiadasDe(pcbAuxiliar);
	mostrarTablaDeArchivosDe(pcbAuxiliar);
	mostrarCantHeapUtilizadasDe(pcbAuxiliar); //tmb muestra 4.a y 4.b cant de acciones allocar y liberar
	mostrarCantSyscallsUtilizadasDe(pcbAuxiliar);


	//todo: se hace free de pcbaux,no?

}

void mostrarCantRafagasEjecutadasDe(tPCB *pcb){

	//todo: ampliar pcb con la cant de rafagas totales?
	printf("Cantidad de rafagas ejecutadas: x!!!!!!x\n");
}
void mostrarCantPrivilegiadasDe(tPCB *pcb){

	printf("Cantidad de operaciones privilegiadas: x!!!!!!x\n");

}
void mostrarTablaDeArchivosDe(tPCB *pcb){


	printf("Tabla de archivos abiertos del proceso: x!!!!!!x\n");
}
void mostrarCantHeapUtilizadasDe(tPCB *pcb){
	printf("Cantidad de paginas de heap utilizacdas: x!!!!!!x\n");

	printf("Cantidad de acciones allocar realizadas: x!!!!!!x\n");


	printf("Cantidad de bytes allocados: x!!!!!!x\n");

	printf("Cantidad de acciones liberar realizados: x!!!!!!x\n");

	printf("Cantidad de bytes liberados: x!!!!!!x\n");
}
void mostrarCantSyscallsUtilizadasDe(tPCB *pcb){

	printf("Cantidad de syscalls utilizadas : x!!!!!!!x\n");
}

void cambiarGradoMultiprogramacion(int nuevoGrado){
	printf("vamos a cambiar el grado a %d\n",nuevoGrado);
	//todo: semaforo
	grado_mult=nuevoGrado;
}
void finalizarProceso(int pidAFinalizar){
	printf("vamos a finalizar el proceso %d\n",pidAFinalizar);



}
void mostrarTablaGlobal(){
	puts("Mostrar tabla global");
}
void stopKernel(){
	puts("Stop kernel");
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

void* queue_get(t_queue *self,int posicion) {
	return list_get(self->elements, posicion);
}
