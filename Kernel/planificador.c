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


t_queue *New, *Exit, *Block,*Ready;
t_list	*cpu_exec,*Exec;


int grado_mult;
extern tKernel *kernel;

void setupPlanificador(void){

	grado_mult = kernel->grado_multiprog;

	New = queue_create();
	Ready = queue_create();
	Exit = queue_create();

	Exec = list_create();
	Block = queue_create();

	cpu_exec = list_create();

}

void mandarPCBaCPU(tPCB *nuevoPCB,int sock_cpu){

	int pack_size, stat;
	puts("Creamos memoria para la variable\n");
	tPackHeader head = { .tipo_de_proceso = KER, .tipo_de_mensaje = PCB_EXEC };
	puts("Comenzamos a serializar el PCB");

	printf("Valor del id del pcb %d\n",nuevoPCB->id);

	char *pcb_serial = serializePCB(nuevoPCB, head, &pack_size);

	printf("pack_size: %d\n", pack_size);

	puts("Enviamos el PCB a CPU");
	if ((stat = send(sock_cpu, pcb_serial, pack_size, 0)) == -1)
		perror("Fallo envio de PCB a CPU. error");

	printf("Se enviaron %d de %d bytes a CPU\n", stat, pack_size);

	list_add(cpu_exec,sock_cpu);
	printf("Se agrego sock_cpu #%d a lista ",sock_cpu);

	freeAndNULL((void **) &pcb_serial);
}

void planificar(){

	grado_mult = kernel->grado_multiprog;
	tPCB * pcbAux;
	t_cpu * cpu;

	while(1){

	switch(kernel->algo){

	case (FIFO):
		printf("Estoy en fifo\n");
		if(!queue_is_empty(New)){
			pcbAux = (tPCB*) queue_pop(New);
			if(grado_mult > 0){
				grado_mult --;
				queue_push(Ready,pcbAux);
			}
		}

		if(!queue_is_empty(Ready)){
			pcbAux = (tPCB*) queue_pop(Ready);
			if(list_size(listaDeCpu) > 0) list_add(Exec,pcbAux);
			}

		if(!list_is_empty(Exec)){
			int i;
			for(i = 0; i < list_size(listaDeCpu);i){
				cpu = (t_cpu *) list_get(listaDeCpu,i);
				pcbAux = (tPCB *) list_get(Exec,i);
				cpu->pid = pcbAux-> id;
				cpu->disponibilidad = OCUPADO;
				mandarPCBaCPU(pcbAux,cpu->fd_cpu);
			}
		}
		//Para saber que hacer con BLOCK y EXIT, recibo mensajes de las cpus activas
		int i,pcb_serial;
		for(i = 0; i < list_size(cpu_exec); i ++){
			int * cpu = (int *) list_get(cpu_exec,i);
			tPackHeader  header;

			int head = recv(*cpu,&header,HEAD_SIZE,0);
			if(head == -1) printf("No se pudo recibir el mensaje");

			if((pcb_serial = recvGeneric(*cpu)) == NULL){
					printf("Fallo recvPCB");
					break;
						}
			pcbAux = deserializarPCB(pcb_serial);

			switch(header.tipo_de_mensaje){

				case(RECURSO_NO_DISPONIBLE):
					queue_push(Block,pcbAux);
					printf("Se agrega pcb a lista de bloqueados por falta de recursos");
					break;
				case(FIN_PROCESO):case(ABORTO_PROCESO): //COLA EXIT
					queue_push(Exit,pcbAux);
					printf("Se finaliza un proceso, se agrega a la cola exit");
					//ConsolaAsociada()
					//LiberarCpuAsociada()
					//LiberarMemoriaDelPrograma()
					//Esta cola solo sirve para almacenar pcb (enunciado)
					break;
				default:
					break;
					}
			if(!queue_is_empty(Block)){
				pcbAux = (tPCB *) queue_pop(Block);
				queue_push(Ready,pcbAux);
				//Todo: tengo que pensar como saber si están o no disponibles los recursos
			}


				}
	break;
	case (RR):

		setearQuamtumS();
		//(pcb *) queue_pop(Ready);
		//list_add(Exec, pcb);

		break;
			}
	free(cpu);cpu = NULL;
	free(pcbAux); pcbAux = NULL;
	list_destroy(cpu_exec);
	}

/* Una vez que lo se envia el pcb a la cpu, la cpu debería avisar si se pudo ejecutar todo o no
 *
 * */}


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


	freeAndNULL((void **) &pack_pid);


	queue_push(New,nuevoPCB);
	planificar();
/*
	puts("Creamos memoria para la variable");
	tPackHeader head = {.tipo_de_proceso = KER, .tipo_de_mensaje = PCB_Exec};
>>>>>>> planificadorKernel
	puts("Comenzamos a serializar el PCB");
	char *pcb_serial = serializePCB(nuevoPCB, head, &pack_size);

	printf("pack_size: %d\n", pack_size);

	puts("Enviamos el PCB a CPU");
	if ((stat = send(sock_cpu, pcb_serial, pack_size, 0)) == -1)
		perror("Fallo envio de PCB a CPU. error");

	printf("Se enviaron %d de %d bytes a CPU\n", stat, pack_size);

	freeAndNULL((void **) &pack_pid);
<<<<<<< HEAD
	//freeAndNULL((void **) &pcb_serial); //todo: falla por 'double free or corruption'; pero ni idea que implica...
=======
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
    freePCBs(Exec);  queue_destroy(Exec);
	freePCBs(Block); queue_destroy(Block);
	freePCBs(Exit);  queue_destroy(Exit);

}
char *serializePCB(tPCB *pcb, tPackHeader head, int *pack_size){

	int off = 0;
	char *pcb_serial;
	bool hayEtiquetas = (pcb->etiquetaSize > 0)? true : false;

	size_t ctesInt_size         = CTES_INT_PCB * sizeof (int);
	size_t indiceCod_size       = pcb->cantidad_instrucciones * 2 * sizeof(int);
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
	memcpy(pcb_serial + off, &pcb->estado_proc, sizeof (int));
	off += sizeof (int);
	memcpy(pcb_serial + off, &pcb->contextoActual, sizeof (int));
	off += sizeof (int);
	memcpy(pcb_serial + off, &pcb->exitCode, sizeof (int));
	off += sizeof (int);

	// serializamos indice de codigo
	memcpy(pcb_serial + off, pcb->indiceDeCodigo, indiceCod_size);
	off += indiceCod_size;

	// serializamos indice de stack
	char *stack_serial = serializarStack(pcb, indiceStack_size, pack_size);
	memcpy(pcb_serial + off, stack_serial, *pack_size);
	off += *pack_size;

	// serializamos indice de etiquetas
	if (hayEtiquetas){
		memcpy(pcb_serial + off, pcb->indiceDeEtiquetas, pcb->etiquetaSize);
		off += sizeof pcb->etiquetaSize;
	}

	memcpy(pcb_serial + HEAD_SIZE, &off, sizeof(int));
	*pack_size = off;

	freeAndNULL((void **) &stack_serial);
	return pcb_serial;
}


char *serializarStack(tPCB *pcb, int pesoStack, int *pack_size){

	int pesoExtra = sizeof(int) + list_size(pcb->indiceDeStack) * 2 * sizeof (int);

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
	*pack_size += off;

	if (!stack_size)
		return stack_serial; // no hay mas stack que serializar, retornamos

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
