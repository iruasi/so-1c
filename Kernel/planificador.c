#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>

#include <commons/collections/queue.h>

#include "planificador.h"

#include <tiposRecursos/misc/pcb.h>
#include <tiposRecursos/tiposPaquetes.h>
#include <tiposRecursos/tiposErrores.h>
#include <funcionesCompartidas/funcionesCompartidas.h>


// TODO: crear esta funcion, que recibe al PCB y lo mete en la cola de NEW...
// Ademas, podria avisar a Consola del PID de este proceso que ya ha sido creado...
// Crear as funciones para el control de los semaforos de los planificadores



/*
 * Se podria manejar la planificacion actuando en base a eventos. Cada vez que un evento sucede,
 * los planificadores apropiados se ejecutan.
 *
 */

extern int *sock_cpu;

t_queue *New, *Ready, *Exec, *Block, *Exit;

int multiprog;
char *sched_policy; // algoritmo de planificacion: FIFO || RR

/* Configura las variables que los planificadores van a ir necesitando
 */
void setupPlanificador(int multiprogramming, char *algorithm){

	sched_policy = malloc(sizeof *algorithm);
	sched_policy = algorithm;
	multiprog = multiprogramming;
}


void largoPlazo(int multiprog){

	if(queue_size(Ready) < multiprog){
		// controlamos que nadie mas este usando este recurso
		queue_push(Ready, queue_pop(New));

	} else if(queue_size(Ready) > multiprog){
		// controlamos que el programa se termine de ejecutar
		queue_push(Exit, queue_pop(Ready));
	}
}


void cortoPlazo(){}

void encolarPrograma(tPCB *nuevoPCB, int sock_con){

	int stat;
//	queue_push(New, (void *) nuevoPCB);


	tPackPID *pack_pid = malloc(sizeof *pack_pid);
	pack_pid->head.tipo_de_proceso = KER;
	pack_pid->head.tipo_de_mensaje = RECV_PID;
	pack_pid->pid = nuevoPCB->id;

	if ((stat = send(sock_con, pack_pid, sizeof(pack_pid), 0)) == -1)
		perror("Fallo envio de PID a Consola. error");

	tPackPCBaCPU * pcb_enviable = malloc(sizeof *pcb_enviable); // TODO: renombrar porque es mentira!
	pcb_enviable->head.tipo_de_proceso = KER;
	pcb_enviable->head.tipo_de_mensaje = PCB_EXEC;
	pcb_enviable->pid = nuevoPCB->id;
	pcb_enviable->pc = nuevoPCB->pc;
	pcb_enviable->pages = nuevoPCB->paginasDeCodigo;
	pcb_enviable->exit = nuevoPCB->exitCode;

	void *pcb_serializado = serializarPCBACpu(pcb_enviable);
	int packSize = sizeof pcb_enviable->head + sizeof pcb_enviable->exit+ sizeof pcb_enviable->pages+ sizeof pcb_enviable->pid+ sizeof pcb_enviable->pc;

/*	int pcbSize = sizeof(tProceso) + sizeof(tMensaje) + sizeof(nuevoPCB->id) + sizeof(nuevoPCB->pc)+sizeof(nuevoPCB->paginasDeCodigo)+ sizeof(nuevoPCB->exitCode);

	char *buffer = malloc(pcbSize);

	int off = 0;
	memcpy(buffer + off, &pcb_enviable->head.tipo_de_proceso, sizeof (tProceso));
	off += sizeof (tProceso);
	memcpy(buffer + off, &pcb_enviable->head.tipo_de_mensaje, sizeof (tMensaje));
	off += sizeof (tMensaje);
	memcpy(buffer + off, &(pcb_enviable->pid), sizeof (pcb_enviable->pid));
	off += sizeof (pcb_enviable->pid);
	memcpy(buffer + off, &(pcb_enviable->pc), sizeof (pcb_enviable->pc));
	off += sizeof (pcb_enviable->pc);
	memcpy(buffer + off, &(pcb_enviable->pages), sizeof (pcb_enviable->pages));
	off += sizeof (pcb_enviable->pages);
	memcpy(buffer + off, &(pcb_enviable->exit), sizeof (pcb_enviable->exit));
	off += sizeof (pcb_enviable->exit);
*/
	printf("%d",sock_cpu[0]);

	if ((stat = send(6, pcb_serializado, packSize, 0)) < 0){
		printf("%d",stat);
		//perror("No se pudo enviar pcb a CPU a Kernel. error");

		}


	freeAndNULL(pack_pid);

}

void updateQueue(t_queue *Q){



}

void freePCBs(t_queue *queue){

	tPCB* pcb;
	puts("Liberando todos los PCBs de la cola...");
	while(queue_size(queue) > 0){

		pcb = (tPCB *) queue_pop(queue);
		freeAndNULL(pcb->indiceDeCodigo);
		//freeAndNULL(pcb->indiceDeEtiquetas);
		//freeAndNULL(pcb->indiceDeStack);
		freeAndNULL(pcb);
	}
}

void limpiarPlanificadores(){
	freePCBs(New);   queue_destroy(New);
	freePCBs(Ready); queue_destroy(Ready);
	freePCBs(Exec);  queue_destroy(Exec);
	freePCBs(Block); queue_destroy(Block);
	freePCBs(Exit);  queue_destroy(Exit);
}

void moverAColaReady(tPCB * proceso){

/*	int *yaEstaReady;
	yaEstaReady = malloc(sizeof(int));
	*yaEstaReady = 0;

	switch(proceso->estado){
	case NEW:
//		mutexLock(mutexColaNew);
		eliminarDeCola(New, proceso);
//		mutexUnlock(mutexColaNew);
		break;
	case READY:
		*yaEstaReady = 1;
		break;
	case EXEC:
//		mutexLock(mutexColaExec);
		eliminarDeCola(Exec, proceso);
//		mutexUnlock(mutexColaExec);
		break;
	case BLOCK:
//		mutexLock(mutexColaBlock);
		eliminarDeCola(Block, proceso);
//		mutexUnlock(mutexColaBlock);
		break;
	}

	if (*yaEstaReady == 0){
		proceso->estado = READY;
//		mutexLock(mutexColaReady);
		queue_push(Ready, proceso);
//		mutexUnlock(mutexColaReady);
	}

	*///free(yaEstaReady);
}

void moverAColaExec(tPCB * proceso){
/*
//	mutexLock(mutexColaReady);
	eliminarDeColaReady(Ready , proceso);
//	mutexUnlock(mutexColaReady);
	//proceso->estado = EXEC;

//	mutexLock(mutexColaExec);
	queue_push(Exec , proceso);
//	mutexUnlock(mutexColaExec);

}

void moverAColaBlock(tPCB* proceso) {

//	mutexLock(mutexColaExec);
	eliminarDeCola(Exec, proceso);
//	mutexUnlock(mutexColaExec);

	proceso->estado = BLOCK;

//	mutexLock(mutexColaBlock);44
	agregarEnCoka(Block, proceso);
//	mutexUnlock(mutexColaBlock);

}

void moverAColaExit(tPCB *proceso) {

//	mutexLock(mutexColaExec);
	eliminarDeCola(Exec, programa);
//	mutexUnlock(mutexColaExec);

	proceso->estado = EXIT;

//	mutexLock(mutexColaExit);
	queue_push(Exit, proceso);
//	mutexUnlock(mutexColaExit);

}

void eliminarDeCola(t_queue *cola, tPCB *proceso){

	tPCB *procesoAuxiliar;
	procesoAuxiliar = malloc(sizeof(tPCB));
	int *i;
	i = malloc(sizeof(int));

	for (*i = 0; i < queue_size(cola); *i++){

		procesoAuxiliar = queue_peek(cola); //copiamos el primer elemento
		queue_pop(cola);//lo sacamos de a cola para luego obtener el proximo

		if(proceso == procesoAuxiliar)
			i = queue_size(cola); // si es el buscado, sale del for con esto
		else
			queue_push(cola , procesoAuxiliar); //si no es el buscado, lo devuelve a la cola

	}

	free(procesoAuxiliar);
	free(i);

}

int eliminarDeColaReady(t_queue *colaReady, tPCB *proceso){
// Al ser FIFO tanto FCFS como RR pasar de READY a EXEC solo sucede con
// el primer elemento de la lsita, esto comprueba que sea el que solicitamos
//	caso contrario arroja error
	tPCB *procesoAuxiliar;
	procesoAuxiliar = malloc(sizeof(tPCB));
	int *todoOK;
	todoOK = malloc(sizeof(int));
	*procesoAuxiliar = queue_peek(colaReady); //copiamos el primer elemento

		if(proceso == procesoAuxiliar){
			queue_pop(colaReady);
			todoOK = 1;
		}else
			todoOK = 0;

	free(procesoAuxiliar);

	if (todoOK == 0){
		free(todoOK);
		perror("Fallo, el proceso de la cola READY no es el mismo que el solicitado. error");
		return FALLO_MOVERAEXEC;
	}else{
		free(todoOK);
		return 1;
	}
*/}
