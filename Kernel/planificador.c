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


// TODO: crear esta funcion, que recibe al PCB y lo mete en la cola de New...
// Ademas, podria avisar a Consola del PID de este proceso que ya ha sido creado...
// Crear as funciones para el control de los semaforos de los planificadores



/*
 * Se podria manejar la planificacion actuando en base a eventos. Cada vez que un evento sucede,
 * los planificadores apropiados se ejecutan.
 *
 */

extern t_list * listaDeCpu;


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

void mandarPCBaCPU(tPCB *nuevoPCB,int sock_cpu){

	int pack_size, stat;
	puts("Creamos memoria para la variable");
	tPackHeader head = { .tipo_de_proceso = KER, .tipo_de_mensaje = PCB_EXEC };
	puts("Comenzamos a serializar el PCB");
	char *pcb_serial = serializePCB(nuevoPCB, head, &pack_size);

	printf("pack_size: %d\n", pack_size);

	puts("Enviamos el PCB a CPU");
	if ((stat = send(sock_cpu, pcb_serial, pack_size, 0)) == -1)
		perror("Fallo envio de PCB a CPU. error");

	printf("Se enviaron %d de %d bytes a CPU\n", stat, pack_size);

	freeAndNULL((void **) &pcb_serial);
}

void planificar(){
	int i = 0;
	grado_mult = kernel->grado_multiprog;
	tPCB * pcbAux;
	int peticionPaginas;

	t_cpu * cpu;

	pcbAux = (tPCB*) queue_pop(New);

	while(1){

	switch(kernel->algo){

	case (FIFO):

		if(!queue_is_empty(New)){
			pcbAux = (tPCB*) queue_pop(New);
			if(grado_mult > 0){
				grado_mult --;
				queue_push(Ready,pcbAux);
			}

		if(!queue_is_empty(Ready)){
			pcbAux = (tPCB*) queue_pop(Ready);
			if(list_size(listaDeCpu) > 0)
				queue_push(Exec,pcbAux);

			}
		if(!queue_is_empty(Exec)){
			for(i = 0; i < list_size(listaDeCpu);i){
				cpu = (t_cpu *) list_get(listaDeCpu,i);
				pcbAux = (tPCB *) queue_pop(Exec);
				cpu->pid = pcbAux-> id;
				cpu->disponibilidad = OCUPADO;
				mandarPCBaCPU(pcbAux,cpu->fd_cpu);
			}
		//stat = recv(socket,MENSJAE,size,) el size del pcb ;
		if(stat == 1){
			switch(pcb->mensaje)://hacer este campo para el pcb
				case(USEQUAMTUM):
					break;
				case(RECURSONODISPONIBLE):
					break;
				case(TERMINO):
						//todo hacer typedef enum para los distintos cases
		}
		}
		}
	case (RR):

		setearQuamtumS();
		//(pcb *) queue_pop(Ready);
		//list_add(Exec, pcb);

		break;

/* Una vez que lo se envia el pcb a la cpu, la cpu debería avisar si se pudo ejecutar todo o no
 *
 * */}
	}
}

void setearQuamtumS(){
	int i;
	for(i = 0; i < Ready->elements->elements_count ; i++){
		tPCB * pcbReady = (tPCB*) list_get(Ready->elements,i);
	//	pcbReady->quantum = kernel-> quamtum;
	//	pcbReady->quamtumSleep = kernel -> quamtumSleep;

	}
}

int hayCpuDisponible(){
	return list_size(listaDeCpu) > 0;
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

void encolarEnNewPrograma(tPCB *nuevoPCB, int sock_con){
	puts("Se encola el programa");
	int stat, pack_size;
	pack_size = 0;

//	queue_push(New, (void *) nuevoPCB);

	tPackPID *pack_pid = malloc(sizeof *pack_pid);
	pack_pid->head.tipo_de_proceso = KER;
	pack_pid->head.tipo_de_mensaje = RECV_PID;

	pack_pid->pid = nuevoPCB->id;

	char *pid_serial = serializePID(pack_pid);
	if (pid_serial == NULL){
		puts("No se serializo bien");

	}
	printf("Aviso al sock consola %d su numero de PID\n", sock_con);
	if ((stat = send(sock_con, pid_serial, sizeof (tPackPID), 0)) == -1)
		perror("Fallo envio de PID a Consola. error");
	printf("Se enviaron %d bytes a Consola\n", stat);

	freeAndNULL((void **) &pack_pid);


	queue_push(New,nuevoPCB);
	planificar();
/*
	puts("Creamos memoria para la variable");
	tPackHeader head = {.tipo_de_proceso = KER, .tipo_de_mensaje = PCB_Exec};
	puts("Comenzamos a serializar el PCB");
	char *pcb_serial = serializePCB(nuevoPCB, head, &pack_size);

	printf("pack_size: %d\n", pack_size);

	puts("Enviamos el PCB a CPU");
	if ((stat = send(sock_cpu, pcb_serial, pack_size, 0)) == -1)
		perror("Fallo envio de PCB a CPU. error");

	printf("Se enviaron %d de %d bytes a CPU\n", stat, pack_size);

	freeAndNULL((void **) &pack_pid);
	freeAndNULL((void **) &pcb_serial);*/
}

void updateQueue(t_queue *Q){



}

void freePCBs(t_queue *queue){

	tPCB* pcb;
	puts("Liberando todos los PCBs de la cola...");
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
