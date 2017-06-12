#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>

#include <commons/collections/queue.h>

#include "planificador.h"
#include "kernelConfigurators.h"

#include <tiposRecursos/misc/pcb.h>
#include <tiposRecursos/tiposPaquetes.h>
#include <tiposRecursos/tiposErrores.h>
#include <funcionesCompartidas/funcionesCompartidas.h>
#include <funcionesPaquetes/funcionesPaquetes.h>

void pausarPlanif(){

}

extern t_log * logger;

// TODO: crear esta funcion, que recibe al PCB y lo mete en la cola de NEW...
// Ademas, podria avisar a Consola del PID de este proceso que ya ha sido creado...
// Crear as funciones para el control de los semaforos de los planificadores



/*
 * Se podria manejar la planificacion actuando en base a eventos. Cada vez que un evento sucede,
 * los planificadores apropiados se ejecutan.
 *
 */

extern int sock_cpu;

t_queue *New, *Exit, *Ready;
t_list *Exec, *Block;


int grado_mult;
extern tKernel *kernel;

void setupPlanificador(void){

	grado_mult = kernel->grado_multiprog;

	New = queue_create();
	Ready = queue_create();
	Exit = queue_create();

	Exec = list_create();
	Block = list_create();

}

void planificador(){

	tPCB *pcb;

	if (list_size(Exec) >= grado_mult){
		log_info(logger,"No se agrega nada y esperamos a que vayan terminando");

	} else {

	switch(kernel->algo){
	case (FIFO):
		pcb = queue_pop(Ready);
		list_add(Exec, pcb);

		//enviarPCBaCPU(pcb);

		break;
	case (RR):

		break;
	}
	} // cierra else
}

void bloquearProceso(){

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

void encolarEnNEWPrograma(tPCB *nuevoPCB, int sock_con){
	log_info(logger,"Se encola el programa");
	int stat;
//	queue_push(New, (void *) nuevoPCB);

	tPackPID *pack_pid = malloc(sizeof *pack_pid);
	pack_pid->head.tipo_de_proceso = KER;
	pack_pid->head.tipo_de_mensaje = RECV_PID;
	pack_pid->pid = nuevoPCB->id;

	char *pid_serial = serializePID(pack_pid);

	log_info(logger,"Aviso al sock consola %d su numero de PID\n", sock_con);
	if ((stat = send(sock_con, pid_serial, sizeof (tPackPID), 0)) == -1)
		log_error(logger,"Fallo envio de PID a Consola. error");
	log_info(logger,"Se enviaron %d bytes a Consola\n", stat);

	log_info(logger,"Creamos memoria para la variable");
	tPackHeader head = {.tipo_de_proceso = KER, .tipo_de_mensaje = PCB_EXEC};
	tPackPCBSimul *pcb = empaquetarPCBconStruct(head, nuevoPCB);

	log_info(logger,"Comenzamos a serializar el PCB");
	char *pcb_serial = serializarPCBACpu(pcb);

	log_info(logger,"Enviamos el PCB a CPU");

	if ((stat = send(sock_cpu, pcb_serial, sizeof *pcb, 0)) == -1)
		log_error(logger,"Fallo envio de PCB a CPU. error");

	log_info(logger,"Se enviaron %d de %d bytes a CPU\n", stat, sizeof *pcb);

	freeAndNULL((void **) &pack_pid);
	freeAndNULL((void **) &pcb);
}

void updateQueue(t_queue *Q){



}

void freePCBs(t_queue *queue){

	tPCB* pcb;
	log_info(logger,"Liberando todos los PCBs de la cola...");
	while(queue_size(queue) > 0){

		pcb = (tPCB *) queue_pop(queue);
		freeAndNULL((void **) &pcb->indiceDeCodigo);
		//freeAndNULL(pcb->indiceDeEtiquetas);
		//freeAndNULL(pcb->indiceDeStack);
		freeAndNULL((void **) &pcb);
	}
}

void limpiarPlanificadores(){
	freePCBs(New);   queue_destroy(New);
	freePCBs(Ready); queue_destroy(Ready);
//	freePCBs(Exec);  list_destroy(Exec);
//	freePCBs(Block); list_destroy(Block);
	freePCBs(Exit);  queue_destroy(Exit);
}
