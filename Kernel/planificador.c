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


#define FIFO_INST -1

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
char *recvHeader(int sock_in, tPackHeader *header);

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
	pack_size = 0;
	puts("Creamos memoria para la variable\n");
	tPackHeader head = { .tipo_de_proceso = KER, .tipo_de_mensaje = PCB_EXEC };
	puts("Comenzamos a serializar el PCB");

	if(kernel->algo == FIFO){
		nuevoPCB->proxima_rafaga = FIFO_INST;
	}else{
		nuevoPCB->proxima_rafaga = kernel->quantum;
	}

	char *pcb_serial = serializePCB(nuevoPCB, head, &pack_size);

	printf("pack_size: %d\n", pack_size);

	puts("Enviamos el PCB a CPU");
	if ((stat = send(sock_cpu, pcb_serial, pack_size, 0)) == -1)
		perror("Fallo envio de PCB a CPU. error");

	printf("Se enviaron %d de %d bytes a CPU\n", stat, pack_size);

	list_add(cpu_exec,(int *) sock_cpu);
	printf("Se agrego sock_cpu #%d a lista \n",sock_cpu);

	//freeAndNULL((void **) &pcb_serial);
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
			for(i = 0; i < list_size(listaDeCpu);i++){
				cpu = (t_cpu *) list_get(listaDeCpu,i);
				pcbAux = (tPCB *) list_get(Exec,i);
				cpu->pid = pcbAux-> id;
				cpu->disponibilidad = OCUPADO;
				mandarPCBaCPU(pcbAux,cpu->fd_cpu);
			}
		}
		//Para saber que hacer con BLOCK y EXIT, recibo mensajes de las cpus activas
		int i;
		char *paquete_pcb_serial;
		tPackHeader * header;
		for(i = 0; i < list_size(cpu_exec); i ++){
			int * cpu2 = (int *) list_get(cpu_exec,i);

			if((paquete_pcb_serial = recvHeader(cpu->fd_cpu,header)) == NULL){
					printf("Fallo recvPCB");
					break;
						}
			pcbAux = deserializarPCB(paquete_pcb_serial);

			switch(header->tipo_de_mensaje){

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
	int stat;
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
    freePCBs(Exec);  list_destroy(Exec);
	freePCBs(Block); queue_destroy(Block);
	freePCBs(Exit);  queue_destroy(Exit);

}

char *recvHeader(int sock_in, tPackHeader *header){
	puts("Se recibe el paquete serializado..");

	int stat, pack_size;
	char *p_serial;

	if((stat = recv(sock_in, header, sizeof(tPackHeader),0)) <= 0){
		perror("Fallo de recv. error");
		return NULL;
	}
	if ((stat = recv(sock_in, &pack_size, sizeof(int), 0)) <= 0){
		perror("Fallo de recv. error");
		return NULL;
	}

	printf("Paquete de size: %d\n", pack_size);

	if ((p_serial = malloc(pack_size-12)) == NULL){
		printf("No se pudieron mallocar %d bytes para paquete generico\n", pack_size);
		return NULL;
	}

	if ((stat = recv(sock_in, p_serial, pack_size-12, 0)) <= 0){
		perror("Fallo de recv. error");
		return NULL;
	}

	return p_serial;
}

