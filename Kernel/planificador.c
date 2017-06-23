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
		puts("No se agrega nada y esperamos a que vayan terminando");

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
	puts("Se encola el programa");
	int stat, pack_size;
	pack_size = 0;

//	queue_push(New, (void *) nuevoPCB);

	tPackPID *pack_pid = malloc(sizeof *pack_pid);
	pack_pid->head.tipo_de_proceso = KER;
	pack_pid->head.tipo_de_mensaje = PID;

	pack_pid->pid = nuevoPCB->id;

	char *pid_serial = serializePID(pack_pid);
	if (pid_serial == NULL){
		puts("No se serializo bien");

	}

	printf("Aviso al sock consola %d su numero de PID\n", sock_con);
	if ((stat = send(sock_con, pid_serial, sizeof (tPackPID), 0)) == -1)
		perror("Fallo envio de PID a Consola. error");
	printf("Se enviaron %d bytes a Consola\n", stat);

	puts("Creamos memoria para la variable");

	tPackHeader head = {.tipo_de_proceso = KER, .tipo_de_mensaje = PCB_EXEC};
	puts("Comenzamos a serializar el PCB");
	char *pcb_serial = serializePCB(nuevoPCB, head, &pack_size);

	printf("pack_size: %d\n", pack_size);

	puts("Enviamos el PCB a CPU");
	if ((stat = send(sock_cpu, pcb_serial, pack_size, 0)) == -1)
		perror("Fallo envio de PCB a CPU. error");

	printf("Se enviaron %d de %d bytes a CPU\n", stat, pack_size);

	freeAndNULL((void **) &pack_pid);
	//freeAndNULL((void **) &pcb_serial); //todo: falla por 'double free or corruption'; pero ni idea que implica...
}
