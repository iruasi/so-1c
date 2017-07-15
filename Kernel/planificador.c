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

void planificar(void);

void finalizarPrograma(int pid, tPackHeader * header, int socket);
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
char *recvHeader(int sock_in, tPackHeader *header);

int grado_mult;
extern tKernel *kernel;

extern sem_t hayProg;
extern sem_t eventoPlani;
extern sem_t hayCPUs;
extern sem_t codigoEnviado;

void setupSemaforosColas(void){
	pthread_mutex_init(&mux_new,   NULL);
	pthread_mutex_init(&mux_ready, NULL);
	pthread_mutex_init(&mux_exec,  NULL);
	pthread_mutex_init(&mux_block, NULL);

	//pthread_mutex_init(&mux_exec,  NULL);
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

/* Enlaza el CPU y el Programa, por medio del PID del PCB que comparten.
 * Se copian los valores de Programa en CPU y se reasignan los punteros.
 * Se apuntan mutuamente, hasta que se deba reasignar el PCB enlazante.
 */
void asociarProgramaACPU(t_RelCC *cpu){

	t_RelPF *pf = getProgByPID(cpu->cpu.pid);

	cpu->con->fd_con = pf->prog->con->fd_con;
	cpu->con->pid    = pf->prog->con->pid;

	freeAndNULL((void **) &pf->src->bytes);
	freeAndNULL((void **) &pf->src);
	free(pf->prog->con);

	pf->prog->con = cpu->con;
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
				if(!queue_is_empty(New))
				{
					pcbAux = (tPCB*) queue_pop(New);
				if(list_size(Exec) < grado_mult)
					encolarDeNewEnReady(pcbAux);
				}
				if(!queue_is_empty(Ready)){
					if(list_size(listaDeCpu) > 0) { // todo: actualizar esta lista...
						pcbAux = (tPCB*) queue_peek(Ready);
						cpu = (t_RelCC *) list_get(listaDeCpu, obtenerCPUociosa());
						cpu->cpu.pid = pcbAux->id;

						pthread_mutex_lock(&mux_ready);
						pcbAux = (tPCB*) queue_pop(Ready);
						pthread_mutex_unlock(&mux_ready);
						pthread_mutex_lock(&mux_exec);
						list_add(Exec, pcbAux);
						pthread_mutex_unlock(&mux_exec);

						asociarProgramaACPU(cpu);
						mandarPCBaCPU(pcbAux, cpu);
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
				printf("Estoy en RR\n");
				if(!queue_is_empty(New))
				{
					pcbAux = (tPCB*) queue_pop(New);
					if(list_size(Exec) < grado_mult)
						encolarDeNewEnReady(pcbAux);
				}
				if(!queue_is_empty(Ready)){
					if(list_size(listaDeCpu) > 0) { // todo: actualizar esta lista...
						pcbAux = (tPCB*) queue_peek(Ready);
						cpu = (t_RelCC *) list_get(listaDeCpu, obtenerCPUociosa());

						cpu->cpu.pid = pcbAux->id;

						pthread_mutex_lock(&mux_ready);
						pcbAux = (tPCB*) queue_pop(Ready);
						pthread_mutex_unlock(&mux_ready);
						pthread_mutex_lock(&mux_exec);
						list_add(Exec, pcbAux);
						pthread_mutex_unlock(&mux_exec);


						//
						t_RelPF *pf = getProgByPID(cpu->cpu.pid);

							cpu->con->fd_con = pf->prog->con->fd_con;
							cpu->con->pid    = pf->prog->con->pid;
							//freeAndNULL((void **) &pf->src->bytes);
							//freeAndNULL((void **) &pf->src);
							//free(pf->prog->con);


							//esto tiene algun sentido?
//							pf->prog->con = cpu->con;
//							pf->prog->cpu.fd_cpu = cpu->cpu.fd_cpu;
//							pf->prog->cpu.pid    = cpu->cpu.pid;
//
//							t_RelPF *pf2 = getProgByPID(cpu->cpu.pid);



						//
						//pcbAux->proxima_rafaga = kernel->quantum;
						mandarPCBaCPU(pcbAux, cpu);
					}
				}

		break;

		} // cierra Switch

		//		setearQuamtumS();
		//(pcb *) queue_pop(Ready);
		//list_add(Exec, pcb);

	} // cierra While

	//free(cpu);cpu = NULL;
	//free(pcbAux); pcbAux = NULL;
	//list_destroy(cpu_exec);
}

/* Una vez que lo se envia el pcb a la cpu, la cpu debería avisar si se pudo ejecutar todo o no
 *
 * *///}

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

	pthread_mutex_lock(&mux_new);
	queue_push(New, pcb);
	pthread_mutex_unlock(&mux_new);

	sem_post(&eventoPlani);
	//sem_post(&hayProg);
}

t_RelPF *getProgByPID(int pid){
	puts("Obtener Programa mediante PID");

	t_RelPF *pf;
	int i;
	for (i = 0; i < list_size(gl_Programas); ++i){
		pf = list_get(gl_Programas, i);
		if (pid == pf->prog->con->pid)
			return pf;
	}

	printf("No se encontro el programa relacionado al PID %d\n", pid);
	return NULL;
}

void encolarDeNewEnReady(tPCB *pcb){
	printf("Se encola el PID %d en Ready\n", pcb->id);

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

	pthread_mutex_lock(&mux_ready);
	queue_push(Ready, pcb);
	pthread_mutex_unlock(&mux_ready);

	//se esta haciendo 2 post para el mismo prog, cuando pasa a new y dsp cdo pasa a ready.. me pa q no esta bien.. lo comento
	//sem_post(&hayProg);

	metadata_destruir(meta);
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

void finalizarPrograma(int pid, tPackHeader * header, int socket){


	printf("Aviso a memoria que libere la memoria asiganda al proceso\n");

	int* exit_pid = malloc(sizeof exit_pid);
	*exit_pid = pid;
	send(socket,header,sizeof(tPackHeader),0);

	free(exit_pid);exit_pid = NULL;


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

void cpu_handler_planificador(t_RelCC *cpu){ // todo: revisar este flujo de acciones
	tPCB *pcbAux, *pcbPlanif;

	int stat;
	tPackHeader * headerMemoria = malloc(sizeof headerMemoria); //Uso el mismo header para avisar a la memoria y consola
	tPackHeader * headerExitCode = malloc(sizeof headerExitCode);//lo uso para indicar a consola de la forma en q termino el proceso.


	headerExitCode->tipo_de_proceso = KER;

	char *buffer;

	switch(cpu->msj){
	case (FIN_PROCESO):
		puts("Se recibe FIN_PROCESO de CPU");

		buffer = recvGeneric(cpu->cpu.fd_cpu);

		pcbAux = deserializarPCB(buffer);

		pthread_mutex_lock(&mux_exec);
		pcbPlanif = list_remove(Exec, getPCBPositionByPid(pcbAux->id, Exec));
		pthread_mutex_unlock(&mux_exec);

		pcbPlanif->exitCode = pcbAux->exitCode; // todo: que valores nos importan retener?

		pthread_mutex_lock(&mux_exit);
		queue_push(Exit, pcbPlanif); //yo agregaria pcbAux ya q CPU lo modifico y seria mejor guardarse ese.. lo mismo mas adelante en pcbpreempt
		pthread_mutex_unlock(&mux_exit);


		headerMemoria->tipo_de_mensaje = FIN_PROG;
		headerMemoria->tipo_de_proceso = KER;

		if((stat = send(cpu->con->fd_con,headerMemoria,sizeof (tPackHeader),0))<0){
			perror("error al enviar a la consola");
			break;
		}

		if(pcbPlanif->exitCode == 0){
			puts("el programa finalizo correctamente");
			//le avisamos a consola
			headerExitCode->tipo_de_mensaje = pcbPlanif->exitCode;
			if((stat = send(cpu->con->fd_con,headerExitCode,sizeof (tPackHeader),0))<0){
				perror("error al enviar a la consola el exitCode");
				break;
			}
		}



		cpu->cpu.pid = cpu->con->pid = cpu->con->fd_con = -1;
		puts("sem post hay cpu");
		sem_post(&eventoPlani);
		break;


	case(PCB_PREEMPT):
			puts("Termino por fin de quantum");

			buffer = recvGeneric(cpu->cpu.fd_cpu);

			pcbAux = deserializarPCB(buffer);

			pthread_mutex_lock(&mux_exec);
			pcbPlanif = list_remove(Exec, getPCBPositionByPid(pcbAux->id, Exec));
			pthread_mutex_unlock(&mux_exec);

			pthread_mutex_lock(&mux_exit);
			queue_push(Ready, pcbAux);
			pthread_mutex_unlock(&mux_exit);

			cpu->cpu.pid = cpu->con->pid = cpu->con->fd_con = -1;

			sem_post(&eventoPlani);


	break;
		case (ABORTO_PROCESO):


		buffer = recvGeneric(cpu->cpu.fd_cpu);

		pcbAux = deserializarPCB(buffer);

		pthread_mutex_lock(&mux_exec);
		pcbPlanif = list_remove(Exec, getPCBPositionByPid(pcbAux->id, Exec));
		pthread_mutex_unlock(&mux_exec);


		pcbPlanif->exitCode = pcbAux->exitCode;

		pthread_mutex_lock(&mux_exit);
		queue_push(Exit, pcbPlanif);
		pthread_mutex_unlock(&mux_exit);

		//Aviso a memoria
		finalizarPrograma(cpu->cpu.pid, headerMemoria, cpu->con->fd_con);



		headerMemoria->tipo_de_proceso=KER;
		headerMemoria->tipo_de_mensaje = ABORTO_PROCESO;

		if((stat = send(cpu->con->fd_con,headerMemoria,sizeof (tPackHeader),0))<0){
			perror("error al enviar a la consola");
			break;
		}

		headerExitCode->tipo_de_mensaje=pcbAux->exitCode;
		if((stat = send(cpu->con->fd_con,headerExitCode,sizeof (tPackHeader),0))<0){
			perror("error al enviar a la consola el exitCode");
			break;
		}


		sem_post(&eventoPlani);


		break;




//	printf("Se finaliza un proceso, libero memoria y luego consola\n");
//	headerMemoria->tipo_de_mensaje = FIN_PROG;
//	headerMemoria->tipo_de_proceso = KER;
//	//ConsolaAsociada()
//
//	//Aviso a memoria
//	finalizarPrograma(cpu->cpu.pid, headerMemoria, cpu->con->fd_con);
//	// Le informo a la Consola asociada:
//
//	for(j = 0;j<list_size(listaDeCpu);j++){
//		cpu = (t_RelCC *) list_get(listaDeCpu,j);
//		headerMemoria->tipo_de_mensaje = FIN_PROG;
//		headerMemoria->tipo_de_proceso = KER;
//
//		if((stat = send(cpu->con->fd_con,headerMemoria,sizeof (tPackHeader),0))<0){
//			perror("error al enviar a la consola");
//			break;
//		}
//	}
//
//	pcbAux = list_remove(Exec, getPCBPositionByPid(cpu->cpu.pid, Exec));
//	queue_push(Exit,pcbAux);
//
//	//agrego esto aunque no me convence si asi hay q tratar a la multiprogra..
//	//sem_post(&hayCPUs);
//
//
//
//	break;
	default:
		break;
	}
}

void blockByPID(int pid){
	puts("Pasamos proceso (que deberia estar en Exec) a Block");

	int p;
	tPCB* pcb;

	pthread_mutex_lock(&mux_exec);
	if ((p = getPCBPositionByPid(pid, Exec)) == -1){
		printf("No existe el PCB de PID %d en la cola de Exec\n", pid);
		pthread_mutex_unlock(&mux_exec); return;
	}
	pcb = list_remove(Exec, p); pthread_mutex_unlock(&mux_exec);

	pthread_mutex_lock(&mux_block);
	list_add(Block, pcb); pthread_mutex_unlock(&mux_block);
}

void unBlockByPID(int pid){
	puts("Pasamos proceso (que deberia estar en Block) a Ready");

	int p;
	tPCB* pcb;

	pthread_mutex_lock(&mux_block);
	if ((p = getPCBPositionByPid(pid, Block)) == -1){
		printf("No existe el PCB de PID %d en la cola de Block\n", pid);
		pthread_mutex_unlock(&mux_block); return;
	}
	pcb = list_remove(Block, p); pthread_mutex_unlock(&mux_block);

	pthread_mutex_lock(&mux_ready);
	queue_push(Ready, pcb); pthread_mutex_unlock(&mux_ready);
	sem_post(&eventoPlani);
	//sem_post(&hayProg);
}


char *serializePCB(tPCB *pcb, tPackHeader head, int *pack_size){

	int off = 0;
	char *pcb_serial;

	size_t ctesInt_size         = CTES_INT_PCB * sizeof (int);
	size_t indiceCod_size       = pcb->cantidad_instrucciones * 2 * sizeof(int);
	size_t indiceStack_size     = sumarPesosStack(pcb->indiceDeStack) + sizeof(int) +
								  list_size(pcb->indiceDeStack) * 2 * sizeof(int);
	size_t indiceEtiquetas_size = pcb->etiquetas_size;

	if ((pcb_serial = malloc(HEAD_SIZE + sizeof(int) + ctesInt_size + indiceCod_size +
			                 indiceStack_size + indiceEtiquetas_size)) == NULL){
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
	memcpy(pcb_serial + off, &pcb->etiquetas_size, sizeof (int));
	off += sizeof (int);
	memcpy(pcb_serial + off, &pcb->cantidad_etiquetas, sizeof (int));
	off += sizeof (int);
	memcpy(pcb_serial + off, &pcb->cantidad_funciones, sizeof (int));
	off += sizeof (int);
	memcpy(pcb_serial + off, &pcb->proxima_rafaga, sizeof (int));
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
	*pack_size = 0;
	char *stack_serial = serializarStack(pcb, indiceStack_size, pack_size);
	memcpy(pcb_serial + off, stack_serial, *pack_size);
	off += *pack_size;

	// serializamos indice de etiquetas
	if (pcb->cantidad_etiquetas || pcb->cantidad_funciones){
		memcpy(pcb_serial + off, pcb->indiceDeEtiquetas, pcb->etiquetas_size);
		off += pcb->etiquetas_size;
	}

	memcpy(pcb_serial + HEAD_SIZE, &off, sizeof(int));
	*pack_size = off;

	free(stack_serial);
	return pcb_serial;
}
