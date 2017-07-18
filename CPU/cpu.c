#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include <math.h>

#include <commons/config.h>
#include <commons/collections/list.h>

#include <funcionesCompartidas/funcionesCompartidas.h>
#include <funcionesPaquetes/funcionesPaquetes.h>
#include <tiposRecursos/tiposErrores.h>
#include <tiposRecursos/tiposPaquetes.h>
#include <tiposRecursos/misc/pcb.h>

#include "cpuConfigurators.h"
#include "funcionesAnsisop.h"

#include <parser/parser.h>
#include <parser/metadata_program.h>

void pedirInstruccion(int instr_size, int *solics);
void recibirInstruccion(char **linea, int instr_size, int solics);
int *ejecutarPrograma(void);

void set_quantum_sleep(void);
void liberarPCB(void);
void liberarStack(t_list *stack_ind);

int conectarConServidores(tCPU *cpu_data);

int pag_size;
int q_sleep;
float q_sleep_segs;

void set_quantum_sleep(void){
	q_sleep_segs = q_sleep / 1000.0;
	printf("Se cambio la latencia de ejecucion de instruccion a %f segundos\n", q_sleep_segs);
}


int err_exec;            // esta variable global va a retener el codigo de error con el que se rompio una primitiva
sem_t sem_fallo_exec;    // este semaforo activa el err_handler para que maneje la correcta comunicacion del error
pthread_mutex_t mux_pcb; // sincroniza sobre el PCB global, para enviar un PCB roto antes de obtener uno nuevo

void err_handler(void){

	int pack_size;
	char *pcb_serial;
	tPackHeader head = {.tipo_de_proceso = CPU, .tipo_de_mensaje = ABORTO_PROCESO};

	while(1){
	sem_wait(&sem_fallo_exec); // este semaforo se va a sem_post'ear por quien detecte algun error
	pthread_mutex_lock(&mux_pcb);
	pcb->exitCode = err_exec;
	printf("Fallo ejecucion del PID %d con el error numero %d\n", pcb->id, pcb->exitCode);

	if ((pcb_serial = serializePCB(pcb, head, &pack_size)) == NULL){
		pthread_mutex_unlock(&mux_pcb); break;
	}

	if (send(sock_kern, pcb_serial, pack_size, 0) == -1){
		perror("Fallo envio de PCB a Kernel. error");
		pthread_mutex_unlock(&mux_pcb); break;
	}
	puts("Se envio el PCB abortado a Kernel");

	freeAndNULL((void **) &pcb_serial);
	liberarPCB();
	puts(" *** El CPU vuelve a un estado consistente y listo para recibir un pcb nuevo ***\n");

	pthread_mutex_unlock(&mux_pcb);
}} // el While va a permanecer por tanto tiempo como exista el hilo main()...

int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	int stat;
	int *retval;

	tCPU *cpu_data = getConfigCPU(argv[1]);
	mostrarConfiguracionCPU(cpu_data);

	setupCPUFunciones();
	setupCPUFuncionesKernel();

	sem_init(&sem_fallo_exec,  0, 0);
	pthread_mutex_init(&mux_pcb, NULL);
	pthread_t pcb_th;
	pthread_t errores_th;
	pthread_create(&errores_th, NULL, (void *) err_handler, NULL);

	if ((stat = conectarConServidores(cpu_data)) != 0){
		puts("No se pudo conectar con ambos servidores");
		return FALLO_GRAL;
	}

	tPackHeader *head = malloc(sizeof *head);
	char *pcb_serial;

	while((stat = recv(sock_kern, head, sizeof *head, 0)) > 0){
		puts("Se recibio un paquete de Kernel");

		printf("proc %d \t msj %d \n", head->tipo_de_proceso, head->tipo_de_mensaje);

		if (head->tipo_de_mensaje == FIN){
			puts("Kernel se va!");
			liberarConfiguracionCPU(cpu_data);

		} else if (head->tipo_de_mensaje == PCB_EXEC){


			if((pcb_serial = recvGeneric(sock_kern)) == NULL){
				return FALLO_RECV;
			}

			pthread_mutex_lock(&mux_pcb);
			pcb = deserializarPCB(pcb_serial);
			pthread_mutex_unlock(&mux_pcb);

			puts("Recibimos un PCB para ejecutar...");
			pthread_create(&pcb_th, NULL, (void *) ejecutarPrograma, NULL);

			pthread_join(pcb_th, (void **) &retval);
			printf("Termino la ejecucion del PCB con valor retorno = %d\n", *retval);

		} else {
			puts("Me re fui");
			return ABORTO_CPU;
		}
	}

	if (stat == -1){
		perror("Error en la recepcion con Kernel. error");
		return FALLO_RECV;
	}

	printf("Kernel termino la conexion\nLimpiando proceso...\n");
	close(sock_kern);
	close(sock_mem);
	free(pcb);
	free(head);
	liberarConfiguracionCPU(cpu_data);
	return 0;
}


int *ejecutarPrograma(void){ sleep(1);

	tPackHeader header;
	int *retval = malloc(sizeof(int));
	int stat, solics = 0;
	t_size instr_size;
	char **linea = malloc(0);
	*linea = NULL;
	bool fin_quantum = false;
	termino = false;
	int cantidadDeRafagas = 0;

	puts("Empieza a ejecutar...");
	do{
		instr_size = (pcb->indiceDeCodigo + pcb->pc)->offset;

		//LEE LA PROXIMA LINEA DEL PROGRAMA
		pedirInstruccion(instr_size, &solics);

		recibirInstruccion(linea, instr_size, solics);

		printf("La linea %d es: %s\n", (pcb->pc+1), *linea);
		//ANALIZA LA LINEA LEIDA Y EJECUTA LA FUNCION ANSISOP CORRESPONDIENTE
		analizadorLinea(*linea, &functions, &kernel_functions);

		cantidadDeRafagas++;
		pcb->pc++;

		if(cantidadDeRafagas == pcb->proxima_rafaga){
			fin_quantum = true;
			break;
		}
		sleep(q_sleep_segs);

	} while(!termino);
	termino = false;


	if (pcb->pc == pcb->cantidad_instrucciones){
		puts("Termino de ejecutar...");
		header.tipo_de_mensaje = FIN_PROCESO;
		*retval = pcb->exitCode = 0; // exit_success

	} else if(fin_quantum == true){ // se dealoja el PCB, faltandole ejecutar instrucciones
		puts("Fin de quantum...");
		header.tipo_de_mensaje = PCB_PREEMPT;
		*retval = PCB_PREEMPT;
	}

	else{
		puts("Aborto proceso...");
		*retval = header.tipo_de_mensaje = ABORTO_PROCESO;
	}
	header.tipo_de_proceso = CPU;

	int pack_size = 0;
	char *pcb_serial = serializePCB(pcb, header, &pack_size);
	printf("Se serializo bien el pcb con tamanio: %d\n", pack_size);
	if ((stat = send(sock_kern, pcb_serial, pack_size, 0)) == -1)
		perror("Fallo envio de PCB a Kernel. error");

	printf("Se enviaron %d bytes a kernel \n", stat);

	free(linea);
	free(pcb_serial);
	return retval;
}

void pedirInstruccion(int instr_size, int *solics){
	puts("Pide instruccion");

	tPackHeader head;
	tPackByteReq *pbrq;
	char *bytereq_serial;
	int i, stat, pack_size, code_page, offset;
	code_page = (pcb->indiceDeCodigo + pcb->pc)->start / pag_size;
	offset    = (pcb->indiceDeCodigo + pcb->pc)->start % pag_size;

	head.tipo_de_proceso = CPU; head.tipo_de_mensaje = BYTES;

	t_list *pedidos = list_create();
	// cantidad de Solicitudes a responder
	*solics = ceil((float) ((pcb->indiceDeCodigo + pcb->pc)->start + instr_size) / pag_size) - code_page;

	for (i = 0; i < *solics; ++i){
		pbrq = malloc(sizeof *pbrq);
		memcpy(&pbrq->head, &head, HEAD_SIZE);
		pbrq->pid    = pcb->id;
		pbrq->page   = code_page + i;
		pbrq->offset = offset;
		pbrq->size   = (offset + instr_size > pag_size)? pag_size - offset : instr_size;

		list_add(pedidos, pbrq);
		offset = 0; // luego del primer pedido, el offset siempre es 0
		instr_size -= pbrq->size;
	}

	for (i = 0; i < *solics; ++i){
		pbrq = list_get(pedidos, i);

		pack_size = 0;
		if ((bytereq_serial = serializeByteRequest(pbrq, &pack_size)) == NULL){
			err_exec = FALLO_SERIALIZAC;
			sem_post(&sem_fallo_exec);
			pthread_exit(&err_exec);
		}

		if((stat = send(sock_mem, bytereq_serial, pack_size, 0)) == -1){
			perror("Fallo envio de paquete de pedido de bytes. error");
			err_exec = FALLO_SEND;
			sem_post(&sem_fallo_exec);
			pthread_exit(&err_exec);
		}
		freeAndNULL((void **) &pbrq);
		freeAndNULL((void **) &bytereq_serial);
	}
	list_destroy(pedidos);
}

void recibirInstruccion(char **linea, int instr_size, int solics){
	puts("vamos a recibir instruccion");

	int stat, i, offset;
	char *byte_serial;
	tPackHeader head;
	tPackBytes *instr;


	//rompe el RR aca

	if ((*linea = realloc(*linea, instr_size)) == NULL){
		printf("No se pudo reallocar %d bytes memoria para la siguiente linea de instruccion\n", instr_size);
		err_exec = FALLO_GRAL;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	for (offset = i = 0; i < solics; ++i){

		if ((stat = recv(sock_mem, &head, HEAD_SIZE, 0)) == -1){
			perror("Fallo recepcion de header. error");
			err_exec = FALLO_RECV;
			sem_post(&sem_fallo_exec);
			pthread_exit(&err_exec);
		}

		if (head.tipo_de_proceso != MEM || head.tipo_de_mensaje != BYTES){
			printf("Se esperaban bytes de Memoria, pero se recibio de %d el mensaje %d\n",
					head.tipo_de_proceso, head.tipo_de_mensaje);
			err_exec = CONEX_INVAL;
			sem_post(&sem_fallo_exec);
			pthread_exit(&err_exec);
		}

		if ((byte_serial = recvGeneric(sock_mem)) == NULL){
			err_exec = FALLO_RECV;
			sem_post(&sem_fallo_exec);
			pthread_exit(&err_exec);
		}

		if ((instr = deserializeBytes(byte_serial)) == NULL){
			err_exec = FALLO_DESERIALIZAC;
			sem_post(&sem_fallo_exec);
			pthread_exit(&err_exec);
		}

		memcpy(*linea + offset, instr->bytes, instr->bytelen);
		offset += instr->bytelen - 1;

		freeAndNULL((void **) &byte_serial);
		freeAndNULL((void **) &instr);
	}
}

int conectarConServidores(tCPU *cpu_data){

	int stat;
	tPackHeader h_esp;

	printf("Conectando con memoria...\n");
	sock_mem = establecerConexion(cpu_data->ip_memoria, cpu_data->puerto_memoria);
	if (sock_mem < 0){
		puts("Fallo conexion a Memoria");
		return FALLO_CONEXION;
	}

	if ((stat = handshakeCon(sock_mem, cpu_data->tipo_de_proceso)) < 0){
		fprintf(stderr, "No se pudo hacer hadshake con Memoria\n");
		return FALLO_GRAL;
	}
	printf("Se enviaron: %d bytes a MEMORIA\n", stat);
	puts("Me conecte a memoria!");

	h_esp.tipo_de_proceso = MEM; h_esp.tipo_de_mensaje = MEMINFO;
	if ((stat = recibirInfoProcSimple(sock_mem, h_esp, &pag_size)) != 0){
		puts("No se pudo recibir size de frame de Memoria!");
		return ABORTO_CPU;
	}
	printf("Se trabaja con un tamanio de pagina de %d bytes!\n", pag_size);
	printf("Me conecte a MEMORIA en socket #%d\n",sock_mem);

	printf("Conectando con kernel...\n");
	sock_kern = establecerConexion(cpu_data->ip_kernel, cpu_data->puerto_kernel);
	if (sock_kern < 0){
		puts("Fallo conexion a Kernel");
		return FALLO_CONEXION;
	}

	if ((stat = handshakeCon(sock_kern, cpu_data->tipo_de_proceso)) < 0){
		fprintf(stderr, "No se pudo hacer hadshake con Kernel\n");
		return FALLO_GRAL;
	}
	printf("Se enviaron: %d bytes a KERNEL\n", stat);

	h_esp.tipo_de_proceso = KER; h_esp.tipo_de_mensaje = KERINFO;
	if ((stat = recibirInfoProcSimple(sock_kern, h_esp, &q_sleep)) != 0){
		puts("No se pudo recibir el quantum sleep de Kernel!");
		return ABORTO_CPU;
	}
	set_quantum_sleep();
	printf("Me conecte a kernel (socket %d)\n", sock_kern);

	return 0;
}

void liberarStack(t_list *stack_ind){

	int i, j;
	indiceStack *stack;

	for (i = 0; i < list_size(stack_ind); ++i){
		stack = list_remove(stack_ind, i);

		for (j = 0; j < list_size(stack->args); ++j)
			list_remove(stack->args, j);
		list_destroy(stack->args);

		for (j = 0; j < list_size(stack->vars); ++j)
			list_remove(stack->vars, j);
		list_destroy(stack->vars);
	}
	list_destroy(stack_ind);
}

void liberarPCB(void){

	free(pcb->indiceDeCodigo);
	if (pcb->indiceDeEtiquetas) // non-NULL
		free(pcb->indiceDeEtiquetas);
	liberarStack(pcb->indiceDeStack);
	freeAndNULL((void **) &pcb);
}
