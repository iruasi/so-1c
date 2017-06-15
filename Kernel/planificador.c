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

char *serializarStack(tPCB *pcb, int pesoStack, int *pack_size);

void pausarPlanif(){

}


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
	freeAndNULL((void **) &pcb_serial);
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

char *serializePCB(tPCB *pcb, tPackHeader head, int *pack_size){

	int off = 0;
	char *pcb_serial;
	bool hayEtiquetas = (pcb->etiquetaSize > 0)? true : false;

	size_t ctesInt_size         = 6 * sizeof (int);
	size_t indiceCod_size       = sizeof (t_puntero_instruccion) + sizeof (t_size);
	size_t indiceStack_size     = sumarPesosStack(pcb->indiceDeStack);
	size_t indiceEtiquetas_size = (size_t) pcb->etiquetaSize;

	if ((pcb_serial = malloc(HEAD_SIZE + sizeof(int) + ctesInt_size + indiceCod_size + indiceStack_size + indiceEtiquetas_size)) == NULL){
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
	memcpy(pcb_serial + off, &pcb->etiquetaSize, sizeof (int));
	off += sizeof (int);
	memcpy(pcb_serial + off, &pcb->cantidad_instrucciones, sizeof (int));
	off += sizeof (int);
	memcpy(pcb_serial + off, &pcb->exitCode, sizeof (int));
	off += sizeof (int);

	// serializamos indice de codigo
	memcpy(pcb_serial + off, &pcb->indiceDeCodigo->start, sizeof pcb->indiceDeCodigo->start);
	off += sizeof pcb->indiceDeCodigo->start;
	memcpy(pcb_serial + off, &pcb->indiceDeCodigo->offset, sizeof pcb->indiceDeCodigo->offset);
	off += sizeof pcb->indiceDeCodigo->offset;


	// serializamos indice de stack
	if (list_size(pcb->indiceDeStack) > 0){
		char *stack_serial = serializarStack(pcb, indiceStack_size, pack_size);
		memcpy(pcb_serial + off, stack_serial, indiceStack_size);
		off += indiceStack_size;
	}

	// serializamos indice de etiquetas
	if (hayEtiquetas){
		memcpy(pcb_serial + off, pcb->indiceDeEtiquetas, pcb->etiquetaSize);
		off += sizeof pcb->etiquetaSize;
	}

	memcpy(pcb_serial + HEAD_SIZE, &off, sizeof(int));
	*pack_size = off;

	return pcb_serial;
}

/* Retorna el size de todas las listas sumadas del stack
 */
int sumarPesosStack(t_list *stack){

	int i, sum;
	indiceStack *temp;

	for (i = sum = 0; i < list_size(stack); ++i){
		temp = list_get(stack, i);
		sum += list_size(temp->args) * sizeof (posicionMemoria) + list_size(temp->vars) * sizeof (posicionMemoriaPid)
				+ sizeof temp->retPos + sizeof temp->retVar;
	}

	return sum;
}


char *serializarStack(tPCB *pcb, int pesoStack, int *pack_size){

	int pesoExtra = list_size(pcb->indiceDeStack) * 2 * sizeof (int);

	char *stack_serial;
	if ((stack_serial = malloc(pesoStack + pesoExtra)) == NULL){
		puts("No se pudo mallocar espacio para el stack serializado");
		return NULL;
	}

	indiceStack *stack;
	posicionMemoria *arg;
	posicionMemoriaPid *var;
	int args_size, vars_size, stack_size;
	int i, j, off;



	stack_size = list_size(pcb->indiceDeStack);
	memcpy(stack_serial, &stack_size, sizeof(int));
	off = sizeof (int);

	for (i = 0; i < stack_size; ++i){
		stack = list_get(pcb->indiceDeStack, i);

		args_size = list_size(stack->args);
		memcpy(stack_serial + off, &args_size, sizeof(int));
		off += sizeof(int);
		for(j = 0; j < args_size; j++){
			arg = list_get(stack->args, j);
			memcpy(stack_serial + off, &arg, sizeof (posicionMemoria));
			off += sizeof (posicionMemoria);
		}

		vars_size = list_size(stack->vars);
		memcpy(stack_serial, &vars_size, sizeof(int));
		off += sizeof (int);
		for(j = 0; j < vars_size; j++){
			var = list_get(stack->vars, j);
			memcpy(stack_serial + off, &var, sizeof (posicionMemoriaPid));
			off += sizeof (posicionMemoriaPid);
		}

		memcpy(stack_serial + off, &stack->retPos, sizeof(int));
		off += sizeof (int);

		memcpy(stack_serial + off, &stack->retVar, sizeof(posicionMemoria));
		off += sizeof (posicionMemoria);
	}

	*pack_size += off;
	return stack_serial;
}

