#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>
#include <semaphore.h>

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

void planificar(void);

void finalizarPrograma(int pid, tPackHeader * header, int socket);
void setearQuamtumS(void);
void pausarPlanif(){

}
int getPosPid(int pid, t_list *Q);
int obtenerCPUociosa(void);

t_list * listaDeCpu;
extern t_list * listaPcb;


t_queue *New, *Exit, *Block,*Ready;
t_list	*cpu_exec,*Exec;
char *recvHeader(int sock_in, tPackHeader *header);

int grado_mult;
extern tKernel *kernel;

extern sem_t *hayProg;

void setupPlanificador(void){

	grado_mult = kernel->grado_multiprog;

	New = queue_create();
	Ready = queue_create();
	Exit = queue_create();

	Exec = list_create();
	Block = queue_create();

	listaDeCpu = list_create();
	cpu_exec   = list_create();

	sem_wait(hayProg);
	planificar();
}

void mandarPCBaCPU(tPCB *nuevoPCB, t_cpuInfo * cpu){

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
	if ((stat = send(cpu->cpu.fd_cpu, pcb_serial, pack_size, 0)) == -1)
		perror("Fallo envio de PCB a CPU. error");

	printf("Se enviaron %d de %d bytes a CPU\n", stat, pack_size);

	//list_add(cpu_exec, cpu);
	printf("Se agrego sock_cpu #%d a lista \n",cpu->cpu.fd_cpu);

	//freeAndNULL((void **) &pcb_serial);
}

void planificar(void){

	grado_mult = kernel->grado_multiprog;
	tPCB * pcbAux;
	t_cpuInfo * cpu;

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
			if(list_size(listaDeCpu) > 0) {
				cpu = (t_cpuInfo *) list_get(listaDeCpu, obtenerCPUociosa());
				cpu->cpu.pid = pcbAux->id;
				list_add(Exec, pcbAux);
				mandarPCBaCPU(pcbAux,cpu);
			}
		}
		break;

		//Para saber que hacer con BLOCK y EXIT, recibo mensajes de las cpus activas
		/*int i;
		int j;
		char *paquete_pcb_serial;
		int stat;
		tPackHeader * header = malloc(HEAD_SIZE);
		tPackHeader * headerMemoria = malloc(sizeof headerMemoria);
		t_cpuInfo p;*/


			/*if((paquete_pcb_serial = recvHeader(cpu_executing->fd_cpu, header)) == NULL){
					printf("Fallo recvPCB");
					break;
			}
			*/
			//pcbAux = deserializarPCB(paquete_pcb_serial);

			/*switch(header->tipo_de_mensaje){


				case(FIN_PROCESO):case(ABORTO_PROCESO):case(RECURSO_NO_DISPONIBLE): //COLA EXIT
					//queue_push(Exit,pcbAux);

					printf("Se finaliza un proceso, libero memoria y luego consola\n");
					headerMemoria->tipo_de_mensaje = FIN_PROG;
					headerMemoria->tipo_de_proceso = KER;
					//ConsolaAsociada()

					//Aviso a memoria
					finalizarPrograma(cpu->cpu.pid, headerMemoria, cpu->con.fd_con);
					// Le informo a la Consola asociada:

					for(j = 0;j<list_size(listaProgramas);j++){
						consolaAsociada = (t_consola *) list_get(listaProgramas,j);
						if(consolaAsociada->pid == cpu->cpu.pid){
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
		}
			if(!queue_is_empty(Block)){
				pcbAux = (tPCB *) queue_pop(Block);
				queue_push(Ready,pcbAux);*/
				//Todo: tengo que pensar como saber si están o no disponibles los recursos
			//}

	case (RR):
		break;

				}

		setearQuamtumS();
		//(pcb *) queue_pop(Ready);
		//list_add(Exec, pcb);

			}

	//free(cpu);cpu = NULL;
	//free(pcbAux); pcbAux = NULL;
	//list_destroy(cpu_exec);
	}

/* Una vez que lo se envia el pcb a la cpu, la cpu debería avisar si se pudo ejecutar todo o no
 *
 * *///}


void setearQuamtumS(void){
	int i;
	for(i = 0; i < Ready->elements->elements_count ; i++){
		tPCB * pcbReady = (tPCB*) list_get(Ready->elements,i);
	//	pcbReady->quantum = kernel-> quamtum;
	//	pcbReady->quamtumSleep = kernel -> quamtumSleep;

	}
}
void cpu_handler_planificador(t_cpuInfo * cpu){
	tPCB *pcbAux;
	int j;
	int stat;
	tPackHeader * headerMemoria = malloc(sizeof headerMemoria); //Uso el mismo header para avisar a la memoria y consola

	switch(cpu->msj){


	case(FIN_PROCESO):case(ABORTO_PROCESO):case(RECURSO_NO_DISPONIBLE): //COLA EXIT
		//queue_push(Exit,pcbAux);

			printf("Se finaliza un proceso, libero memoria y luego consola\n");
	headerMemoria->tipo_de_mensaje = FIN_PROG;
	headerMemoria->tipo_de_proceso = KER;
	//ConsolaAsociada()

	//Aviso a memoria
	finalizarPrograma(cpu->cpu.pid, headerMemoria, cpu->con->fd_con);
	// Le informo a la Consola asociada:

	for(j = 0;j<list_size(listaDeCpu);j++){
		cpu = (t_cpuInfo *) list_get(listaDeCpu,j);
		headerMemoria->tipo_de_mensaje = FIN_PROG;
		headerMemoria->tipo_de_proceso = KER;

		if((stat = send(cpu->con->fd_con,headerMemoria,sizeof (tPackHeader),0))<0){
			perror("error al enviar a la consola");
			break;
		}
	}

	pcbAux = list_remove(Exec, getPosPid(cpu->cpu.pid, Exec));
	queue_push(Exit,pcbAux);

	break;
	default:
		break;
	}
}

int getPosPid(int pid, t_list *Q){

	int i;
	tPCB *pcb;
	for (i = 0; i < list_size(Q); ++i){
		pcb = list_get(Q, i);
		if (pcb->id == pid)
			return i;
	}

	return -1;

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


	t_cpuInfo *cpu_inex = malloc(sizeof *cpu_inex);
	cpu_inex->con = malloc(sizeof *cpu_inex->con);
	cpu_inex->con->fd_con = sock_con;
	cpu_inex->cpu.pid = -1;
	list_add(listaDeCpu, cpu_inex);

	queue_push(New,nuevoPCB);

	freeAndNULL((void **) &pack_pid);
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

void pasarABlock(int sock_cpu){


}

int obtenerCPUociosa(void){
	t_cpuInfo * cpuOciosa;
	int cantidadCpu;
	for(cantidadCpu = 1; cantidadCpu< list_size(listaDeCpu); cantidadCpu ++){
		cpuOciosa = (t_cpuInfo *) list_get(listaDeCpu,cantidadCpu);
		if(cpuOciosa->cpu.pid < 0)
			return cantidadCpu;
	}
	return -1;
}
