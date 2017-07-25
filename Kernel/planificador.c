#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>

#include <commons/collections/queue.h>
#include <commons/log.h>

#include "defsKernel.h"
#include "planificador.h"
#include "kernelConfigurators.h"
#include "auxiliaresKernel.h"


#include <tiposRecursos/misc/pcb.h>
#include <tiposRecursos/tiposPaquetes.h>
#include <tiposRecursos/tiposErrores.h>
#include <funcionesCompartidas/funcionesCompartidas.h>
#include <funcionesPaquetes/funcionesPaquetes.h>

#define FIFO_INST -1

extern int sock_mem;
extern int frame_size;
extern t_list *gl_Programas, *listaDeCpu;

t_queue *New, *Exit, *Ready;
t_list	*cpu_exec, *Exec, *Block;
t_list *listaProgramas;
extern t_list *finalizadosPorConsolas;
extern t_log * logger;

int grado_mult;
extern tKernel *kernel;

sem_t eventoPlani;
pthread_mutex_t mux_new, mux_ready, mux_exec, mux_block, mux_exit, mux_listaDeCPU,mux_gradoMultiprog;;
extern pthread_mutex_t mux_gl_Programas;

void pausarPlanif(void){

}

void setupGlobales_planificador(void){
	pthread_mutex_init(&mux_new,   NULL);
	pthread_mutex_init(&mux_ready, NULL);
	pthread_mutex_init(&mux_exec,  NULL);
	pthread_mutex_init(&mux_block, NULL);

	sem_init(&eventoPlani, 0, 0);
}

void setupPlanificador(void){
	MUX_LOCK(&mux_gradoMultiprog);
	grado_mult = kernel->grado_multiprog;
	MUX_UNLOCK(&mux_gradoMultiprog);
	New   = queue_create();
	Ready = queue_create();
	Exit  = queue_create();

	Exec  = list_create();
	Block = list_create();

	cpu_exec   = list_create();
	listaProgramas = list_create();

	planificar();
}

void mandarPCBaCPU(tPCB *pcb, t_RelCC * cpu){

	int pack_size, stat;
	pack_size = 0;
	tPackHeader head = { .tipo_de_proceso = KER, .tipo_de_mensaje = PCB_EXEC };
	log_trace(logger, "Comenzamos a serializar el PCB");

	pcb->proxima_rafaga = (kernel->algo == FIFO)? FIFO_INST : kernel->quantum;

	char *pcb_serial;
	if ((pcb_serial = serializePCB(pcb, head, &pack_size)) == NULL){
		puts("Fallo serializacion de pcb");
		return;
	}
	printf("pack_size: %d\n", pack_size);

	puts("Enviamos el PCB a CPU");
	if ((stat = send(cpu->cpu.fd_cpu, pcb_serial, pack_size, 0)) == -1){
		perror("Fallo envio de PCB a CPU. error");
		return;
	}
	printf("Se enviaron %d bytes a CPU\n", pack_size);

	printf("Se agrego sock_cpu #%d a lista \n",cpu->cpu.fd_cpu);

	free(pcb_serial);
}

void asociarProgramaACPU(t_RelCC *cpu){

	t_RelPF *pf = getProgByPID(cpu->cpu.pid);

	cpu->con->fd_con = pf->prog->con->fd_con;
	cpu->con->pid    = pf->prog->con->pid;

	pf->prog->cpu.fd_cpu = cpu->cpu.fd_cpu;
	pf->prog->cpu.pid    = cpu->cpu.pid;
}

void planificar(void){

	tPCB * pcbAux;
	t_RelCC * cpu;
	int pos;

	while(1)
	{
		puts("Wait evento plani");
		sem_wait(&eventoPlani);
		puts("Pase evento plani");
		switch(kernel->algo){
		case (FIFO):
			printf("Estoy en fifo\n");

			MUX_LOCK(&mux_new); MUX_LOCK(&mux_exec);MUX_LOCK(&mux_gradoMultiprog);
			if(!queue_is_empty(New)){ // todo: list_size(Exec) + list_size(Ready) < grado_mult?
				if(list_size(Exec) < grado_mult && obtenerCPUociosa() != -1){
					pcbAux = (tPCB*) queue_pop(New);
					encolarDeNewEnReady(pcbAux);
				}
			}
			MUX_UNLOCK(&mux_new); MUX_UNLOCK(&mux_exec);MUX_UNLOCK(&mux_gradoMultiprog);

			MUX_LOCK(&mux_ready); MUX_LOCK(&mux_exec); MUX_LOCK(&mux_listaDeCPU);
			if(!queue_is_empty(Ready) && (pos = obtenerCPUociosa()) != -1){

				cpu = (t_RelCC *) list_get(listaDeCpu, pos);
				pcbAux = (tPCB*) queue_pop(Ready);
				cpu->cpu.pid = pcbAux->id;

				list_add(Exec, pcbAux);
				asociarProgramaACPU(cpu);
				mandarPCBaCPU(pcbAux, cpu);
			}
			MUX_UNLOCK(&mux_ready); MUX_UNLOCK(&mux_exec); MUX_UNLOCK(&mux_listaDeCPU);
			break;

		case (RR):
			printf("Estoy en RR\n");

			MUX_LOCK(&mux_new); MUX_LOCK(&mux_exec);
			if(!queue_is_empty(New)){
				if(list_size(Exec) < grado_mult){ // todo: list_size(Exec) + list_size(Ready) < grado_mult?
					pcbAux = (tPCB*) queue_pop(New);
					encolarDeNewEnReady(pcbAux);
				}
			}
			MUX_UNLOCK(&mux_new); MUX_UNLOCK(&mux_exec);

			MUX_LOCK(&mux_ready); MUX_LOCK(&mux_exec); MUX_LOCK(&mux_listaDeCPU);
			if(!queue_is_empty(Ready) && (pos = obtenerCPUociosa()) != -1){

				cpu = (t_RelCC *) list_get(listaDeCpu, pos);
				pcbAux = (tPCB*) queue_pop(Ready);
				cpu->cpu.pid = pcbAux->id;

				list_add(Exec, pcbAux);
				asociarProgramaACPU(cpu);
				mandarPCBaCPU(pcbAux, cpu);
			}
			MUX_UNLOCK(&mux_ready); MUX_UNLOCK(&mux_exec); MUX_UNLOCK(&mux_listaDeCPU);
		break;

		} // cierra Switch
	} // cierra While
}

int getPCBPositionByPid(int pid, t_list *cola_pcb){

	int i, size;
	tPCB *pcb;

	size = list_size(cola_pcb);
	for (i = 0; i < size; ++i){
		pcb = list_get(cola_pcb, i);
		if (pcb->id == pid)
			return i;
	}
	return -1;
}

void encolarEnNew(tPCB *pcb){
	puts("Se encola el programa");

	MUX_LOCK(&mux_new);
	queue_push(New, pcb);
	MUX_UNLOCK(&mux_new);

	sem_post(&eventoPlani);
}

t_RelPF *getProgByPID(int pid){
	puts("Obtener Programa mediante PID");

	t_RelPF *pf;
	int i;
	MUX_LOCK(&mux_gl_Programas);
	for (i = 0; i < list_size(gl_Programas); ++i){
		pf = list_get(gl_Programas, i);
		if (pid == pf->prog->con->pid){
			MUX_UNLOCK(&mux_gl_Programas);
			return pf;
		}
	}
	MUX_UNLOCK(&mux_gl_Programas);

	printf("No se encontro el programa relacionado al PID %d\n", pid);
	return NULL;
}

void encolarDeNewEnReady(tPCB *pcb){
	printf("Se encola el PID %d en Ready\n", pcb->id);
	if(getProgByPID(pcb->id) != NULL){ //por las dudas de q no encuentre el programa relacinoado;por ej pcb esperando en new y consola se desocnecta
		t_RelPF *pf = getProgByPID(pcb->id);

		t_metadata_program *meta = metadata_desde_literal(pf->src->bytes);
		t_size indiceCod_size = meta->instrucciones_size * sizeof(t_intructions);
		int code_pages = (int) ceil((float) pf->src->bytelen / frame_size);

		pcb->pc                     = meta->instruccion_inicio;
		pcb->paginasDeCodigo        = code_pages;
		pcb->etiquetas_size         = meta->etiquetas_size;
		pcb->cantidad_etiquetas     = meta->cantidad_de_etiquetas;
		pcb->cantidad_funciones     = meta->cantidad_de_funciones;
		pcb->cantidad_instrucciones = meta->instrucciones_size;


		if ((pcb->indiceDeCodigo    = malloc(indiceCod_size)) == NULL){
			printf("Fallo malloc de %d bytes para pcb->indiceDeCodigo\n", indiceCod_size);
			return;
		} memcpy(pcb->indiceDeCodigo, meta->instrucciones_serializado, indiceCod_size);

		if (pcb->cantidad_etiquetas || pcb->cantidad_funciones){
			if ((pcb->indiceDeEtiquetas = malloc(meta->etiquetas_size)) == NULL){
				printf("Fallo malloc de %d bytes para pcb->indiceDeEtiquetas\n", meta->etiquetas_size);
				return;
			}
			memcpy(pcb->indiceDeEtiquetas, meta->etiquetas, pcb->etiquetas_size);
		}

		avisarPIDaPrograma(pf->prog->con->pid, pf->prog->con->fd_con);
		iniciarYAlojarEnMemoria(pf, code_pages + kernel->stack_size);
		crearInfoProcess(pcb->id);

		MUX_LOCK(&mux_ready);
		queue_push(Ready, pcb);
		MUX_UNLOCK(&mux_ready);

		metadata_destruir(meta);
	}
	else
	{
		puts("SACAMOS Y ELIMINAMOS EL PCB Q NO TIENE CODIGOFUENTE ASOCIADO");
	}
}

void iniciarYAlojarEnMemoria(t_RelPF *pf, int pages){

	int stat;
	char *pidpag_serial;
 	int pack_size = 0;
	tPackHeader ini = { .tipo_de_proceso = KER, .tipo_de_mensaje = INI_PROG };
	tPackHeader src = { .tipo_de_proceso = KER, .tipo_de_mensaje = ALMAC_BYTES };

	tPackPidPag pp;
	pp.head = ini;
	pp.pid = pf->prog->con->pid;
	pp.pageCount = pages;

	if ((pidpag_serial = serializePIDPaginas(&pp, &pack_size)) == NULL){
		puts("fallo serialize PID Paginas");
		return;
	}

	puts("Enviamos inicializacion de programa a Memoria");
	if ((stat = send(sock_mem, pidpag_serial, pack_size, 0)) == -1){
		puts("Fallo pedido de inicializacion de prog en Memoria...");
		return;
	}

	tPackByteAlmac *pbal;
	if ((pbal = malloc(sizeof *pbal)) == NULL){
		printf("Fallo mallocar %d bytes para pbal\n", sizeof *pbal);
		return;
	}

	memcpy(&pbal->head, &src, HEAD_SIZE);
	pbal->pid    = pf->prog->con->pid;
	pbal->page   = 0;
	pbal->offset = 0;
	pbal->size   = pf->src->bytelen;
	pbal->bytes  = pf->src->bytes;

	char *packBytes; pack_size = 0;
	if ((packBytes = serializeByteAlmacenamiento(pbal, &pack_size)) == NULL){
		puts("fallo serialize Bytes");
		return;
	}

	puts("Enviamos el srccode");
	if ((stat = send(sock_mem, packBytes, pack_size, 0)) == -1)
		puts("Fallo envio src code a Memoria...");
	printf("se enviaron %d bytes\n", stat);

	free(pidpag_serial);
	free(pbal);
	free(packBytes);
}

void avisarPIDaPrograma(int pid, int sock_prog){

	int pack_size, stat;
	char *pid_serial;
	tPackPID pack_pid;

	pack_pid.head.tipo_de_proceso = KER; pack_pid.head.tipo_de_mensaje = PID;
	pack_pid.val = pid;

	pack_size = 0;
	if ((pid_serial = serializeVal(&pack_pid, &pack_size)) == NULL){
		puts("No se serializo bien");
		return;
	}

	printf("Aviso al hilo_consola %d su numero de PID\n", sock_prog);
	if ((stat = send(sock_prog, pid_serial, pack_size, 0)) == -1){
		perror("Fallo envio de PID a Consola. error");
		return;
	}
	printf("Se enviaron %d bytes al hilo_consola\n", stat);

	free(pid_serial);
}

int finalizarEnMemoria(int pid){
	printf("Comando a Memoria que libere las paginas del PID %d\n", pid);

	int pack_size;
	char *ppid_serial;
	tPackPID ppid;

	ppid.head.tipo_de_proceso = KER; ppid.head.tipo_de_mensaje = FIN_PROG;
	ppid.val = pid;

	pack_size = 0;
	if ((ppid_serial = serializeVal(&ppid, &pack_size)) == NULL)
		return FALLO_SERIALIZAC;

	if (send(sock_mem, ppid_serial, pack_size, 0) == -1){
		perror("Fallo send FIN_PROG a Memoria. error");
		return FALLO_SEND;
	}

	liberarHeapEnKernel(pid);

	return 0;
}

int obtenerCPUociosa(void){
	t_RelCC * cpuOciosa;
	int cantidadCpu;
	for(cantidadCpu = 0; cantidadCpu < list_size(listaDeCpu); cantidadCpu ++){
		cpuOciosa = (t_RelCC *) list_get(listaDeCpu,cantidadCpu);
		if(cpuOciosa->cpu.pid == PID_IDLE)
			return cantidadCpu;
	}
	return -1;
}

int fueFinalizadoPorConsola(int pid){

	int i;
	for(i = 0; i < list_size(finalizadosPorConsolas); i++){
		t_finConsola *fcAux = list_get(finalizadosPorConsolas, i);
		if(fcAux->pid == pid)
			return i;
	}
	return -1;
}

void cpu_handler_planificador(t_RelCC *cpu){

	tPackHeader * headerMemoria = malloc(sizeof headerMemoria); //Uso el mismo header para avisar a la memoria y consola
	tPackHeader * headerFin = malloc(sizeof headerFin);//lo uso para indicar a consola de la forma en q termino el proceso.
	tPCB *pcbCPU, *pcbPlanif;

	char *buffer = recvGeneric(cpu->cpu.fd_cpu);
	pcbCPU = deserializarPCB(buffer);

	switch(cpu->msj){
	case (FIN_PROCESO):

	puts("Se recibe FIN_PROCESO de CPU");

	//lo sacamos de la lista de EXEC
	MUX_LOCK(&mux_exec);
	pcbPlanif = list_remove(Exec, getPCBPositionByPid(pcbCPU->id, Exec));
	MUX_UNLOCK(&mux_exec);

	mergePCBs(&pcbPlanif, pcbCPU); // ahora Planif es equivalente a CPU

	encolarEnExit(pcbPlanif,cpu);

	//ponemos la cpu como libre
	cpu->cpu.pid = cpu->con->pid = cpu->con->fd_con = -1;
	puts("eventoPlani (cpudisponible)");
	sem_post(&eventoPlani);


	puts("Fin case FIN_PROCESO");
	break;

	case (PCB_PREEMPT):
		puts("Termino por fin de quantum");

	MUX_LOCK(&mux_exec);
	pcbPlanif = list_remove(Exec, getPCBPositionByPid(pcbCPU->id, Exec));
	MUX_UNLOCK(&mux_exec);

	mergePCBs(&pcbPlanif, pcbCPU);

	MUX_LOCK(&mux_exit);
	queue_push(Ready, pcbCPU);
	MUX_UNLOCK(&mux_exit);

	cpu->cpu.pid = cpu->con->pid = cpu->con->fd_con = -1;
	sem_post(&eventoPlani);

	break;

	case (PCB_BLOCK):
		printf("Se bloquea el PID %d\n", cpu->cpu.pid);
		blockByPID(cpu->cpu.pid, pcbCPU);
		cpu->cpu.pid = -1;

		puts("Fin case PCB_BLOCK");
		sem_post(&eventoPlani);
		break;

	case (ABORTO_PROCESO):
		printf("CPU %d aborto el PID %d\n", cpu->cpu.fd_cpu, cpu->cpu.pid);

		MUX_LOCK(&mux_exec);
		pcbPlanif = list_remove(Exec, getPCBPositionByPid(pcbCPU->id, Exec));
		MUX_UNLOCK(&mux_exec);

		mergePCBs(&pcbPlanif, pcbCPU);

		encolarEnExit(pcbPlanif, cpu);

		puts("Fin case ABORTO_PROCESO");
		sem_post(&eventoPlani);
		break;

	default:
		break;
	}
}

void encolarEnExit(tPCB *pcb,t_RelCC *cpu){

	int q;
	tPackHeader * headerFin = malloc(sizeof headerFin); //lo uso para indicar a consola de q termino el proceso.

	printf("\n\n#####EXIT CODE DEL PROCESO %d: %d\n\n#####", pcb->id, pcb->exitCode);

	if(pcb->exitCode != CONS_DISCONNECT){

		if(pcb->exitCode!= DESCONEXION_CPU){
			headerFin->tipo_de_proceso = KER; headerFin->tipo_de_mensaje = FIN_PROG;
		}
		else{
			headerFin->tipo_de_proceso=KER;headerFin->tipo_de_mensaje=DESCONEXION_CPU;
		}
		informarResultado(cpu->con->fd_con, *headerFin);
	}

	if (finalizarEnMemoria(cpu->cpu.pid) != 0)
		printf("No se pudo pedir finalizacion en Memoria del PID %d\n", cpu->cpu.pid);

	//Agregamos el PCB Devuelto por cpu + el excode corresp a la cola de EXIT
	MUX_LOCK(&mux_exit);
	queue_push(Exit, pcb);
	MUX_UNLOCK(&mux_exit);

	//lo sacamos de la lista gl_Programas:

	MUX_LOCK(&mux_gl_Programas);
	t_RelPF *pf;
	for(q=0;q<list_size(gl_Programas);q++){
		pf=list_get(gl_Programas,q);
		if(pf->prog->con->pid == pcb->id){
			list_remove(gl_Programas,q);
		}
	}
	MUX_UNLOCK(&mux_gl_Programas);
	cpu->cpu.pid = cpu->con->pid = cpu->con->fd_con = -1;
}

void blockByPID(int pid, tPCB *pcbCPU){
	printf("Pasamos PID %d desde Exec a Block\n", pid);

	tPCB *pcbPlanif;

	MUX_LOCK(&mux_exec);
	pcbPlanif = list_remove(Exec, getPCBPositionByPid(pid, Exec));
	MUX_UNLOCK(&mux_exec);

	mergePCBs(&pcbPlanif, pcbCPU);

	MUX_LOCK(&mux_block);
	list_add(Block, pcbPlanif);
	MUX_UNLOCK(&mux_block);
}

/* Se deshace del PCB viejo y lo apunta al nuevo */
void mergePCBs(tPCB **old, tPCB *new){
	liberarPCB(*old);
	*old = new;
}

void unBlockByPID(int pid){
	puts("Pasamos proceso (que deberia estar en Block) a Ready");

	int p;
	tPCB* pcb;

	MUX_LOCK(&mux_block);
	if ((p = getPCBPositionByPid(pid, Block)) == -1){
		printf("No existe el PCB de PID %d en la cola de Block\n", pid);
		MUX_UNLOCK(&mux_block); return;
	}
	pcb = list_remove(Block, p);
	MUX_UNLOCK(&mux_block);

	MUX_LOCK(&mux_ready);
	queue_push(Ready, pcb);
	MUX_UNLOCK(&mux_ready);

	sem_post(&eventoPlani);
}
