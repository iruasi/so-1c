#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>

#include <commons/collections/queue.h>

#include "planificador.h"
#include "kernelConfigurators.h"
#include "auxiliaresKernel.h"


#include <tiposRecursos/misc/pcb.h>
#include <tiposRecursos/tiposPaquetes.h>
#include <tiposRecursos/tiposErrores.h>
#include <funcionesCompartidas/funcionesCompartidas.h>
#include <funcionesPaquetes/funcionesPaquetes.h>


#define FIFO_INST -1

void finalizarPrograma(int pid, tPackHeader * header, int socket);
void setearQuamtumS(void);
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
extern t_list * listaProgramas;
extern t_list * listaPcb;

extern t_consola * consola;

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

void mandarPCBaCPU(tPCB *nuevoPCB,t_cpu * cpu){

	int pack_size, stat;
	pack_size = 0;
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
	if ((stat = send(cpu->fd_cpu, pcb_serial, pack_size, 0)) == -1)
		perror("Fallo envio de PCB a CPU. error");

	printf("Se enviaron %d de %d bytes a CPU\n", stat, pack_size);

	list_add(cpu_exec,cpu);
	printf("Se agrego sock_cpu #%d a lista \n",cpu->fd_cpu);

	//freeAndNULL((void **) &pcb_serial);
}

void planificar(){

	grado_mult = kernel->grado_multiprog;
	tPCB * pcbAux;
	t_cpu * cpu;

	t_consola * consolaAsociada = malloc(sizeof consolaAsociada);
	while(1){

	switch(kernel->algo){

	case (FIFO):
		printf("Estoy en fifo\n");
		if(!queue_is_empty(New)){
			pcbAux = (tPCB*) queue_pop(New);
			if(list_size(Exec) < grado_mult){
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

				mandarPCBaCPU(pcbAux,cpu);
			}
		}
		//Para saber que hacer con BLOCK y EXIT, recibo mensajes de las cpus activas
		int i;
		int j;
		char *paquete_pcb_serial;
		int stat;
		tPackHeader * header = malloc(HEAD_SIZE);
		tPackHeader * headerMemoria = malloc(sizeof headerMemoria); //Uso el mismo header para avisar a la memoria y consola

		for(i = 0; i < list_size(cpu_exec); i ++){
			t_cpu * cpu_executing = (t_cpu*) list_get(cpu_exec,i);

			printf("valor de cpu_executing->fd_cpu:%d \n",cpu_executing->fd_cpu);

			if((paquete_pcb_serial = recvHeader(cpu_executing->fd_cpu, header)) == NULL){
					printf("Fallo recvPCB");
					break;
						}
			pcbAux = deserializarPCB(paquete_pcb_serial);

			switch(header->tipo_de_mensaje){

				//Solo va a block por syscalls a los semaforos
				case(SYSCALL):
					cpu_manejador(cpu_executing->fd_cpu);

				case(FIN_PROCESO):case(ABORTO_PROCESO):case(RECURSO_NO_DISPONIBLE): //COLA EXIT
					//queue_push(Exit,pcbAux);

					printf("Se finaliza un proceso, libero memoria y luego consola\n");
					headerMemoria->tipo_de_mensaje = FIN_PROG;
					headerMemoria->tipo_de_proceso = KER;
					//ConsolaAsociada()

					//Aviso a memoria
					finalizarPrograma(cpu_executing->pid, headerMemoria, consolaAsociada->fd_con);
					// Le informo a la Consola asociada:

					for(j = 0;j<list_size(listaProgramas);j++){
						consolaAsociada = (t_consola *) list_get(listaProgramas,j);
						if(consolaAsociada->pid == cpu_executing->pid){
						headerMemoria->tipo_de_mensaje = FIN_PROG;
						headerMemoria->tipo_de_proceso = KER;
						if((stat = send(consolaAsociada->fd_con,headerMemoria,sizeof (tPackHeader),0))<0){
							perror("error al enviar a la consola");
							break;
						}

						// Libero la Consola asociada y la saco del sistema:
						list_remove(listaProgramas,j);

						free(consolaAsociada);consolaAsociada = NULL;
						}
					}
					list_remove(cpu_exec,i);
					list_add(listaDeCpu,cpu);
					queue_push(Exit,pcbAux);

					break;
				default:
					break;
					}
			/*if(!queue_is_empty(Block)){
				pcbAux = (tPCB *) queue_pop(Block);
				queue_push(Ready,pcbAux);*/
				//Todo: tengo que pensar como saber si están o no disponibles los recursos
			//}


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


void setearQuamtumS(void){
	int i;
	for(i = 0; i < Ready->elements->elements_count ; i++){
		tPCB * pcbReady = (tPCB*) list_get(Ready->elements,i);
	//	pcbReady->quantum = kernel-> quamtum;
	//	pcbReady->quamtumSleep = kernel -> quamtumSleep;

	}
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
	int pack_size;

	tPackPID *pack_pid = malloc(sizeof *pack_pid);
	pack_pid->head.tipo_de_proceso = KER;
	pack_pid->head.tipo_de_mensaje = PID;

	pack_pid->val = nuevoPCB->id;

	pack_size = 0;
	char *pid_serial = serializePID(pack_pid, &pack_size);
	if (pid_serial == NULL){
		puts("No se serializo bien");

	}
	printf("Aviso al sock consola %d su numero de PID\n", sock_con);
	if ((stat = send(sock_con, pid_serial, pack_size, 0)) == -1)
		perror("Fallo envio de PID a Consola. error");
	printf("Se enviaron %d bytes a Consola\n", stat);

	//consola->fd_con = sock_con;
	consola->pid = pack_pid->val;

	printf("El socket de consola #%d y pid #%d \n",consola->fd_con,consola->pid);

	list_add(listaProgramas,consola); //Agrego en la lista segun su numero de pid

	freeAndNULL((void **) &pack_pid);

	queue_push(New,nuevoPCB);
	planificar();
}

int indexPcb(int pid){
	int i;
	tPCB * unPcb = NULL;
	for (i = 0; i < list_size(listaPcb); i++){
		unPcb = (tPCB*) list_get(listaPcb, i);
		if(unPcb->id == pid){
			return i; // el pcb del proceso está en la posición 'i'
		}
	}
	return -1; // no se encontró el proceso
}

void finalizarPrograma(int pid, tPackHeader * header, int socket){


			printf("Aviso a memoria que libere la memoria asiganda al proceso\n");

			int* exit_pid = malloc(sizeof exit_pid);
			*exit_pid = pid;
			send(socket,header,sizeof(tPackHeader),0);

			free(exit_pid);exit_pid = NULL;


	}






/*void limpiarPlanificadores(){
	freePCBs(New);   queue_destroy(New);
	freePCBs(Ready); queue_destroy(Ready);
    freePCBs(Exec);  list_destroy(Exec);
	freePCBs(Block); queue_destroy(Block);
	freePCBs(Exit);  queue_destroy(Exit);

}*/

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

