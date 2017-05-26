#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>

#include <commons/collections/queue.h>

#include "planificador.h"
#include "../Compartidas/pcb.h"
#include "../Compartidas/tiposPaquetes.h"
#include "../Compartidas/funcionesCompartidas.h"

// TODO: crear esta funcion, que recibe al PCB y lo mete en la cola de NEW...
// Ademas, podria avisar a Consola del PID de este proceso que ya ha sido creado...



/*
 * Se podria manejar la planificacion actuando en base a eventos. Cada vez que un evento sucede,
 * los planificadores apropiados se ejecutan.
 *
 */

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
	queue_push(New, (void *) nuevoPCB);


	tPackPID *pack_pid = malloc(sizeof *pack_pid);
	pack_pid->head.tipo_de_proceso = KER;
	pack_pid->head.tipo_de_mensaje = RECV_PID;
	pack_pid->pid = nuevoPCB->id;

	if ((stat = send(sock_con, pack_pid, sizeof(pack_pid), 0)) == -1)
		perror("Fallo envio de PID a Consola. error");
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
		freeAndNULL(pcb->indiceDeEtiquetas);
		freeAndNULL(pcb->indiceDeStack);
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
