#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>

#include <commons/collections/queue.h>

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

void mergePCBs(tPCB *old, tPCB *new);

void planificar(void);

int finalizarEnMemoria(int pid);
void pausarPlanif(){

}

/* Obtiene la posicion en que se encuentra un PCB en una lista,
 * dado el pid por el cual ubicarlo.
 * Es requisito necesario que la lista contenga tPCB*
 * Retorna la posicion si la encuentra.
 * Retorna valor negativo en caso contrario.
 */
int getPCBPositionByPid(int pid, t_list *cola_pcb);
int obtenerCPUociosa(void);

extern int sock_mem;
extern int frame_size;
extern t_list *gl_Programas;
t_list *listaDeCpu; // el cpu_manejador deberia crear el entry para esta lista.

t_queue *New, *Exit, *Ready;
t_list	*cpu_exec, *Exec, *Block;
t_list *listaProgramas;
extern t_list *finalizadosPorConsolas;


int grado_mult;
extern tKernel *kernel;

extern sem_t eventoPlani;

void setupSemaforosColas(void){
	pthread_mutex_init(&mux_new,   NULL);
	pthread_mutex_init(&mux_ready, NULL);
	pthread_mutex_init(&mux_exec,  NULL);
	pthread_mutex_init(&mux_block, NULL);
	pthread_mutex_init(&mux_listaFinalizados,NULL);

}

void setupPlanificador(void){

	grado_mult = kernel->grado_multiprog;

	New   = queue_create();
	Ready = queue_create();
	Exit  = queue_create();

	Exec  = list_create();
	Block = list_create();

	setupSemaforosColas();

	listaDeCpu = list_create();
	cpu_exec   = list_create();
	listaProgramas = list_create();

	planificar();
}

void mandarPCBaCPU(tPCB *pcb, t_RelCC * cpu){

	//sem_wait(&codigoEnviado);
	int pack_size, stat;
	pack_size = 0;
	tPackHeader head = { .tipo_de_proceso = KER, .tipo_de_mensaje = PCB_EXEC };
	puts("Comenzamos a serializar el PCB");

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

	grado_mult = kernel->grado_multiprog;
	tPCB * pcbAux;
	t_RelCC * cpu;

	while(1)
	{
		puts("Wait evento plani");
		sem_wait(&eventoPlani);
		puts("Pase evento plani");
		switch(kernel->algo){
		case (FIFO):
			printf("Estoy en fifo\n");

			MUX_LOCK(&mux_new); MUX_LOCK(&mux_exec);
			if(!queue_is_empty(New)){ // todo: list_size(Exec) + list_size(Ready) < grado_mult?
				if(list_size(Exec) < grado_mult && obtenerCPUociosa() != -1){
					pcbAux = (tPCB*) queue_pop(New);
					encolarDeNewEnReady(pcbAux);
				}
			}
			MUX_UNLOCK(&mux_new); MUX_UNLOCK(&mux_exec);

			if(!queue_is_empty(Ready) && obtenerCPUociosa() != -1){
				if(list_size(listaDeCpu) > 0) {

					cpu = (t_RelCC *) list_get(listaDeCpu, obtenerCPUociosa());

					MUX_LOCK(&mux_ready);
					pcbAux = (tPCB*) queue_pop(Ready);
					MUX_UNLOCK(&mux_ready);

					cpu->cpu.pid = pcbAux->id;

					MUX_LOCK(&mux_exec);
					list_add(Exec, pcbAux);
					MUX_UNLOCK(&mux_exec);

					asociarProgramaACPU(cpu);
					mandarPCBaCPU(pcbAux, cpu);
				}

			}

				break;

		case (RR):
				printf("Estoy en RR\n");
				if(!queue_is_empty(New))
				{
					pcbAux = (tPCB*) queue_pop(New);
					if(list_size(Exec) < grado_mult) // todo: list_size(Exec) + list_size(Ready) < grado_mult?
						encolarDeNewEnReady(pcbAux);
				}
				if(!queue_is_empty(Ready)){
					if(list_size(listaDeCpu) > 0 && obtenerCPUociosa() != -1) { // todo: actualizar esta lista...
						pcbAux = (tPCB*) queue_peek(Ready);

						cpu = (t_RelCC *) list_get(listaDeCpu, obtenerCPUociosa());

						cpu->cpu.pid = pcbAux->id;

						MUX_LOCK(&mux_ready);
						pcbAux = (tPCB*) queue_pop(Ready);
						MUX_UNLOCK(&mux_ready);
						MUX_LOCK(&mux_exec);
						list_add(Exec, pcbAux);
						MUX_UNLOCK(&mux_exec);

						asociarProgramaACPU(cpu);
						mandarPCBaCPU(pcbAux, cpu);
					}
				}

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
		iniciarYAlojarEnMemoria(pf, code_pages);

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

void cpu_handler_planificador(t_RelCC *cpu){ // todo: revisar este flujo de acciones
	tPCB *pcbCPU, *pcbPlanif;

	int stat, q,k;
	tPackHeader * headerMemoria = malloc(sizeof headerMemoria); //Uso el mismo header para avisar a la memoria y consola
	tPackHeader * headerFin = malloc(sizeof headerFin);//lo uso para indicar a consola de la forma en q termino el proceso.
	t_finConsola * fcAux = malloc(sizeof fcAux);



	char *buffer = recvGeneric(cpu->cpu.fd_cpu);
	pcbCPU = deserializarPCB(buffer);

	switch(cpu->msj){
	case (FIN_PROCESO):

	puts("Se recibe FIN_PROCESO de CPU");
	//lo sacamos de la lista de EXEC
	MUX_LOCK(&mux_exec);
	pcbPlanif = list_remove(Exec, getPCBPositionByPid(pcbCPU->id, Exec));
	MUX_UNLOCK(&mux_exec);

	//mergePCBs(pcbPlanif, pcbCPU); // ahora Planif es equivalente a CPU
	//temporal:

	pcbPlanif->rafagasEjecutadas = pcbCPU->rafagasEjecutadas;
	pcbPlanif->exitCode = pcbCPU->exitCode;

	//chequeamos que alguna consola no lo haya finalizado previamente
	pthread_mutex_lock(&mux_listaFinalizados);
	if ((k=fueFinalizadoPorConsola(pcbPlanif->id))!= -1){
		fcAux=list_get(finalizadosPorConsolas,k);
		pcbPlanif->exitCode = fcAux->ecode;
	}
	else{
		pcbPlanif->exitCode = pcbCPU->exitCode;
	}
	pthread_mutex_unlock(&mux_listaFinalizados);

	//avisamos a consola el fin siempre y cuando no haya finalizado antes

	printf("Exit code del proceso %d: %d\n", pcbPlanif->id, pcbPlanif->exitCode);

	if(pcbPlanif->exitCode != CONS_DISCONNECT){

		/*char *ecode_serial;
		int pack_size;
		tPackExitCode pack_ec;

		pack_ec.head.tipo_de_proceso = KER;list_remove
		pack_ec.head.tipo_de_mensaje = FIN_PROG;
		pack_ec.val = pcbPlanif->exitCode;

		pack_size = 0;
		if ((ecode_serial = serializeVal(&pack_ec, &pack_size)) == NULL){
			puts("No se serializo bien");
			return;
		}

		printf("aviso a %d que el proceso %d termino con un exitcode: %d.\n ", cpu->con->fd_con, pcbPlanif->id, pcbPlanif->exitCode);
		if ((stat = send(cpu->con->fd_con, ecode_serial, pack_size, 0)) == -1){ // todo: rompe misterioso
			perror("Fallo envio de a Consola. error");
			return;
		}
		printf("Se enviaron %d bytes al hilo_consola\n", stat);

		free(ecode_serial);*/
		headerFin->tipo_de_proceso=KER;
		headerFin->tipo_de_mensaje=FIN_PROG;
		if((stat = send(cpu->con->fd_con,headerFin,sizeof (tPackHeader),0))<0){
				perror("error al enviar a la consola");
				break;
			}

	}

	if (finalizarEnMemoria(cpu->cpu.fd_cpu) != 0)
		printf("No se pudo pedir finalizacion en Memoria del PID %d\n", cpu->cpu.fd_cpu);

	//Agregamos el PCB Devuelto por cpu + el excode corresp a la cola de EXIT
	MUX_LOCK(&mux_exit);
	//queue_push(Exit, pcbPlanif); //yo agregaria pcbAux ya q CPU lo modifico y seria mejor guardarse ese.. lo mismo mas adelante en pcbpreempt
	queue_push(Exit, pcbPlanif);
	MUX_UNLOCK(&mux_exit);

	//lo sacamos de la lista gl_Programas:

	t_RelPF *pf;
	for(q=0;q<list_size(gl_Programas);q++){
		pf=list_get(gl_Programas,q);
		if(pf->prog->con->pid == pcbPlanif->id){
			list_remove(gl_Programas,q);
		}
	}

	//ponemos la cpu como libre
	cpu->cpu.pid = cpu->con->pid = cpu->con->fd_con = -1;
	puts("eventoPlani (cpudispo)");
	sem_post(&eventoPlani);

	puts("Fin case FIN_PROCESO");
	break;

	case(PCB_PREEMPT):
		puts("Termino por fin de quantum");

	MUX_LOCK(&mux_exec);
	//pcbPlanif = list_remove(Exec, getPCBPositionByPid(pcbAux->id, Exec));
	list_remove(Exec, getPCBPositionByPid(pcbCPU->id, Exec));
	MUX_UNLOCK(&mux_exec);

	MUX_LOCK(&mux_exit);
	queue_push(Ready, pcbCPU);
	MUX_UNLOCK(&mux_exit);

	cpu->cpu.pid = cpu->con->pid = cpu->con->fd_con = -1;

	sem_post(&eventoPlani);

	break;

	case (ABORTO_PROCESO):

				MUX_LOCK(&mux_exec);
	pcbPlanif = list_remove(Exec, getPCBPositionByPid(pcbCPU->id, Exec));
	MUX_UNLOCK(&mux_exec);

	pcbPlanif->exitCode = pcbCPU->exitCode;

	MUX_LOCK(&mux_exit);
	queue_push(Exit, pcbPlanif);
	MUX_UNLOCK(&mux_exit);

	//Aviso a memoria
	if (!finalizarEnMemoria(cpu->cpu.fd_cpu))
		printf("No se pudo pedir finalizacion en Memoria del PID %d\n", cpu->cpu.fd_cpu);

	headerMemoria->tipo_de_proceso=KER;
	headerMemoria->tipo_de_mensaje = ABORTO_PROCESO;

	if((stat = send(cpu->con->fd_con,headerMemoria,sizeof (tPackHeader),0))<0){
		perror("error al enviar a la consola");
		break;
	}

	headerFin->tipo_de_mensaje=pcbCPU->exitCode;
	if((stat = send(cpu->con->fd_con,headerFin,sizeof (tPackHeader),0))<0){
		perror("error al enviar a la consola el exitCode");
		break;
	}

	sem_post(&eventoPlani);
	break;

	default:
		break;
	}
}

void blockByPID(int pid){ //todo: que saque el PCB de ejecucion?
	puts("Pasamos proceso (que deberia estar en Exec) a Block");

	int p;
	tPCB* pcb;

	MUX_LOCK(&mux_exec);
	if ((p = getPCBPositionByPid(pid, Exec)) == -1){
		printf("No existe el PCB de PID %d en la cola de Exec\n", pid);
		MUX_UNLOCK(&mux_exec); return;
	}
	pcb = list_remove(Exec, p); MUX_UNLOCK(&mux_exec);

	MUX_LOCK(&mux_block);
	list_add(Block, pcb); MUX_UNLOCK(&mux_block);
}

/* Se deshace del PCB viejo y lo apunta al nuevo */
void mergePCBs(tPCB *old, tPCB *new){
	liberarPCB(old);
	old = new;
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
	pcb = list_remove(Block, p); MUX_UNLOCK(&mux_block);

	MUX_LOCK(&mux_ready);
	queue_push(Ready, pcb); MUX_UNLOCK(&mux_ready);
	sem_post(&eventoPlani);
	//sem_post(&hayProg);
}
