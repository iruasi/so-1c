#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>

#include <commons/collections/queue.h>

#include "planificador.h"
#include "../Compartidas/pcb.h"
#include "../Compartidas/tiposPaquetes.h"


// TODO: crear esta funcion, que recibe al PCB y lo mete en la cola de NEW...
// Ademas, podria avisar a Consola del PID de este proceso que ya ha sido creado...


t_queue* New;
t_queue* Ready;
t_queue* Exec;
t_queue* Block;
t_queue* Exit;

/*
 * Se podria manejar la planificacion actuando en base a eventos. Cada vez que un evento sucede,
 * los planificadores apropiados se ejecutan.
 *
 */

/* Comienza los procedimientos de planificacion, dado un grado de multiprogramacion
 */
void startScheduling(int multiprog){



}


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
