#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include <math.h>
#include <signal.h>

#include <commons/config.h>
#include <commons/collections/list.h>
#include <commons/log.h>

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
void signal_handler(void);
void sigusr1Handler(void);

int conectarConServidores(tCPU *cpu_data);
t_log * logTrace;
int pag_size, stack_size;
int q_sleep;
float q_sleep_segs;
bool ejecutando;
bool finalizate;
tCPU *cpu_data;

void set_quantum_sleep(void){
	q_sleep_segs = q_sleep / 1000.0;
	printf("Se cambio la latencia de ejecucion de instruccion a %f segundos\n", q_sleep_segs);
}


int err_exec;            // esta variable global va a retener el codigo de error con el que se rompio una primitiva
sem_t sem_fallo_exec;    // este semaforo activa el err_handler para que maneje la correcta comunicacion del error
pthread_mutex_t mux_pcb,mux_ejecutando; // sincroniza sobre el PCB global, para enviar un PCB roto antes de obtener uno nuevo


void err_handler(void){

	int pack_size, stat;
	char *pcb_serial;
	tPackHeader head = {.tipo_de_proceso = CPU, .tipo_de_mensaje = ABORTO_PROCESO};

	while(1){
	sem_wait(&sem_fallo_exec); // este semaforo se va a sem_post'ear por quien detecte algun error
	pthread_mutex_lock(&mux_pcb);
	pcb->exitCode = err_exec;
	printf("Fallo ejecucion del PID %d con el error numero %d\n, enviamos el pcb abortado a kernel.", pcb->id, pcb->exitCode);
	log_error(logTrace,"Fallo de ejecucion del PID %d. Error nro: %d",pcb->id,pcb->exitCode);

	if ((pcb_serial = serializePCB(pcb, head, &pack_size)) == NULL){
		pthread_mutex_unlock(&mux_pcb); break;
	}

	//printf("Tenemos que mandar %d bytes de PCB a Kernel\n", pack_size);
	log_trace(logTrace,"mandamos %d bytes del pcb a  kernel",pack_size);
	if ((stat = send(sock_kern, pcb_serial, pack_size, 0)) == -1){
		perror("Fallo envio de PCB a Kernel. error");
		log_error(logTrace,"fallo envio de pcb a kernel");
		pthread_mutex_unlock(&mux_pcb); break;
	}

	//printf("Se enviaron %d bytes a Kernel\n", stat);
	//puts("Se envio el PCB abortado a Kernel");
	log_trace(logTrace,"se enviardon %d bytes a kernel del pcb abortado",stat);
	freeAndNULL((void **) &pcb_serial);
	liberarPCB(pcb);
	pthread_mutex_unlock(&mux_pcb);
	log_trace(logTrace,"el cpu vuelve a un estado constistente y lsito para recibir un pcb nuevo");
	puts(" *** El CPU vuelve a un estado consistente y listo para recibir un pcb nuevo ***\n");
}} // el While va a permanecer por tanto tiempo como exista el hilo main()...



void signal_handler(void){
	signal(SIGUSR1, sigusr1Handler);
}

void sigusr1Handler(void){

	puts("Señal SIGUSR1 detectada");
	log_trace(logTrace,"Señal SIGUSR1 detectada");
	if(!finalizate){
		if(ejecutando){
			//puts("Esperamos a que termine la rafaga para desconectar esta cpu");
			log_trace(logTrace,"Esperamos q termine la rafaga para desconectar esta CPU");

			finalizate = true;
		}
		else{
			log_trace(logTrace,"No hay rafaga ejecutando, procedemos a desconectar esta cpu");
			finalizate = true;
			close(sock_kern);
			close(sock_mem);
			liberarConfiguracionCPU(cpu_data);
			exit(0);

		}
	}
	return;
}




int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}
	signal(SIGUSR1, sigusr1Handler);
	logTrace = log_create("/home/utnso/logCPUTrace.txt", "CPU", 0, LOG_LEVEL_TRACE);
	log_trace(logTrace,"\n\n\n\n\n Inicia nueva ejecucion de CPU \n\n\n\n\n");

	int stat;
	int *retval;
	ejecutando = false;
	finalizate = false;
	cpu_data = getConfigCPU(argv[1]);
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
		log_error(logTrace,"No se pudo conectar con ambos servidores");
		return FALLO_GRAL;
	}

	//tPackHeader *head = malloc(sizeof *head);
	tPackHeader head;
	char *pcb_serial;
	tPackVal *packNuevoQS;
	char *buffer;

	//pthread_create(&signalH_th, NULL, (void *) signal_handler, NULL);

	while((stat = recv(sock_kern, &head, HEAD_SIZE, 0)) > 0){

		//puts("Se recibio un paquete de Kernel");
		//printf("proc %d \t msj %d \n", head->tipo_de_proceso, head->tipo_de_mensaje);
		//log_trace(logTrace,"Se recibio un paquete de kernel");

		if(head.tipo_de_mensaje == NEW_QSLEEP){
				if ((buffer = recvGeneric(sock_kern)) == NULL){
					log_error(logTrace,"error al recibir el nuevo qs");
					return FALLO_RECV;
				}

				if ((packNuevoQS = deserializeVal(buffer)) == NULL){
					log_error(logTrace, "error al deserializar el packQS");
					return FALLO_DESERIALIZAC;
				}

				log_trace(logTrace, "recibimos mensaje de nuevo quantum_sleep");

				memcpy(&q_sleep, &packNuevoQS->val, sizeof(int));

				set_quantum_sleep();

				freeAndNULL((void **) &packNuevoQS);
				freeAndNULL((void **) &buffer);

				log_trace(logTrace, "NUEVO QS: %d", q_sleep);

				printf("NUEVO QS: %f (flo)\n",  q_sleep_segs);

		}
		else if (head.tipo_de_mensaje == PCB_EXEC){

			ejecutando=true;
			if((pcb_serial = recvGenericWFlags(sock_kern,MSG_WAITALL)) == NULL){
				return FALLO_RECV;
			}

			pthread_mutex_lock(&mux_pcb);
			pcb = deserializarPCB(pcb_serial);
			pthread_mutex_unlock(&mux_pcb);

			puts("Recibimos un PCB para ejecutar...");
			log_trace(logTrace,"recibimos un pcb para ejecutar");
			pthread_create(&pcb_th, NULL, (void *) ejecutarPrograma, NULL);

			pthread_join(pcb_th, (void **) &retval);
			printf("Termino la ejecucion del PCB con valor retorno = %d\n", *retval);
			log_trace(logTrace,"termino la ejecucion del PCB con valor retorno");

			if(finalizate == true){
				puts("Fin de ejecucion, nos desconectamos por el SIGUSR1 detectado");
				log_trace(logTrace,"Nos desconectamos por el SIGUSR1 detectado antes");
				break;
			}

		} else {
			//puts("Me re fui");
			//return ABORTO_CPU; todo:por las dudas no abortaría ... y pondria un recibi cualqui
			//puts("recibi un mensaje no considerado");
			log_trace(logTrace, "Recibi un msj no considerado");
		}
	}

	if (stat == -1 && finalizate == false){
		perror("Error en la recepcion con Kernel. error");
		log_error(logTrace,"error en la recepcion c kernel");
		log_trace(logTrace,"se limpia el proceso y se cierra..");
		//puts("Se limpia el proceso y se cierra..");
		close(sock_kern);
		close(sock_mem);
		//free(head);
		ejecutando=false;
		liberarConfiguracionCPU(cpu_data);
		return FALLO_GRAL;
	}

	if(finalizate == false){
		log_trace(logTrace,"kernel termino la conexion, limpiando proceso..");
		printf("Kernel termino la conexion\nLimpiando proceso...\n");
		close(sock_kern);
		close(sock_mem);
		liberarConfiguracionCPU(cpu_data);

	}

	//free(head);



	//pthread_join de los hilos de error y signal?..


	return 0;
}


int *ejecutarPrograma(void){sleep(1);

	tPackHeader header;
	int *retval = malloc(sizeof(int));
	int stat, solics = 0;
	t_size instr_size;
	char *linea = NULL;
	fin_quantum = false;
	termino = bloqueado = false;
	int cantidadDeRafagas = 0;


	puts("Empieza a ejecutar...");
	log_trace(logTrace,"empieza a ejecutar [PID %d]",pcb->id);
	ejecutando=true;
	do{
		instr_size = (pcb->indiceDeCodigo + pcb->pc)->offset;

		//LEE LA PROXIMA LINEA DEL PROGRAMA
		log_trace(logTrace,"pide instruccion[PID %d]",pcb->id);
		pedirInstruccion(instr_size, &solics);
		log_trace(logTrace,"recibe instruccion[PID %d]",pcb->id);
		recibirInstruccion(&linea, instr_size, solics);

		printf("La linea %d es: %s\n", (pcb->pc+1), linea);
		log_trace(logTrace,"La linea %d es: %s [PID %d]",(pcb->pc+1),linea,pcb->id);
		log_trace(logTrace,"ejecuta la instruccion[PID %d]",pcb->id);
		//ANALIZA LA LINEA LEIDA Y EJECUTA LA FUNCION ANSISOP CORRESPONDIENTE
		analizadorLinea(linea, &functions, &kernel_functions);

		cantidadDeRafagas++;
		pcb->pc++;

		pcb->rafagasEjecutadas++;

		if(cantidadDeRafagas == pcb->proxima_rafaga){
			fin_quantum = true;
			break;
		}
		sleep(q_sleep_segs);

	} while(!termino && !bloqueado);

	if (pcb->pc == pcb->cantidad_instrucciones){
		log_trace(logTrace,"Termino de ejecutar, exit_success");
		puts("Termino de ejecutar...EXIT_SUCCESS");
		header.tipo_de_mensaje = FIN_PROCESO;
		*retval = pcb->exitCode = 0; // exit_success

	} else if (bloqueado){
		puts("El PCB se bloquea...");
		log_trace(logTrace,"El pcb se bloquea PCB_BLOCK");
		*retval = header.tipo_de_mensaje = PCB_BLOCK;

	} else if(fin_quantum == true && finalizate == true){

		puts("S1G1");
		*retval=header.tipo_de_mensaje=SIG1;

	}else if (fin_quantum == true){ // se dealoja el PCB, faltandole ejecutar instrucciones
		puts("Fin de quantum...");
		log_trace(logTrace,"Desaloja por fin de quantum PCB_PREEMPT");
		*retval = header.tipo_de_mensaje = PCB_PREEMPT;
	}

	else{
		puts("Aborto proceso...");
		log_trace(logTrace,"aborto proceos");
		*retval = header.tipo_de_mensaje = ABORTO_PROCESO;
	}
	header.tipo_de_proceso = CPU;

	int pack_size = 0;
	char *pcb_serial = serializePCB(pcb, header, &pack_size);
	//printf("Se serializo bien el pcb con tamanio: %d\n", pack_size);
	log_trace(logTrace,"Se serializo bien el pcb con tamanio %d [PID %d]",pack_size,pcb->id);
	if ((stat = send(sock_kern, pcb_serial, pack_size, 0)) == -1){
		log_error(logTrace,"fallo envio de pcb a kernel [PID %d]",pcb->id);
		perror("Fallo envio de PCB a Kernel. error");
	}
	puts("se envio el pcb a kernel");
	log_trace(logTrace,"Se enviaron %d bytes a kernel del pcb [PID %d]",stat,pcb->id);

	pthread_mutex_lock(&mux_ejecutando);
	ejecutando = false;
	pthread_mutex_unlock(&mux_ejecutando);
	free(linea);

	free(pcb_serial);
	liberarPCB(pcb);
	return retval;
}

void pedirInstruccion(int instr_size, int *solics){

	//puts("Pide instruccion");

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
			log_error(logTrace,"fallo envio de paquete de pedido de bytes");
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
	//puts("vamos a recibir instruccion");

	int stat, i, offset;
	char *byte_serial;
	tPackHeader head;
	tPackBytes *instr;



	if ((*linea = realloc(*linea, instr_size + 1)) == NULL){
		log_error(logTrace,"no se pudieron reallocar %d bytes memoria para la sig linea de instruccino",instr_size);
		printf("No se pudo reallocar %d bytes memoria para la siguiente linea de instruccion\n", instr_size);
		err_exec = FALLO_GRAL;
		sem_post(&sem_fallo_exec);
		pthread_exit(&err_exec);
	}

	for (offset = i = 0; i < solics; ++i){

		if ((stat = recv(sock_mem, &head, HEAD_SIZE, 0)) == -1){
			log_error(logTrace,"Fallo recepcion de header");
			perror("Fallo recepcion de header. error");
			err_exec = FALLO_RECV;
			sem_post(&sem_fallo_exec);
			pthread_exit(&err_exec);
		}

		if (head.tipo_de_proceso != MEM || head.tipo_de_mensaje != BYTES){
			printf("Se esperaban bytes de Memoria, pero se recibio de %d el mensaje %d\n",
					head.tipo_de_proceso, head.tipo_de_mensaje);
			log_error(logTrace,"se esperaban bytes de memoria pero se recibio otro tipo de msj");
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

	//printf("Conectando con memoria...\n");
	log_trace(logTrace,"conectando con memoria");
	sock_mem = establecerConexion(cpu_data->ip_memoria, cpu_data->puerto_memoria);
	if (sock_mem < 0){
		//puts("Fallo conexion a Memoria");
		log_error(logTrace,"fallo conexion a memoria");
		return FALLO_CONEXION;
	}

	if ((stat = handshakeCon(sock_mem, cpu_data->tipo_de_proceso)) < 0){
		log_error(logTrace,"no se pudo hacer handshake con memoria");
		fprintf(stderr, "No se pudo hacer hadshake con Memoria\n");
		return FALLO_GRAL;
	}

	//printf("Se enviaron: %d bytes a MEMORIA\n", stat);
	log_trace(logTrace,"se enviaron %d bytes a MEMORIA",stat);
	puts("Me conecte a memoria!");

	h_esp.tipo_de_proceso = MEM; h_esp.tipo_de_mensaje = MEMINFO;
	if ((stat = recibirInfoProcSimple(sock_mem, h_esp, &pag_size)) != 0){
		log_error(logTrace,"no se pudo recibir size de frame de memoria");
		puts("No se pudo recibir size de frame de Memoria!");
		return ABORTO_CPU;
	}

	printf("Se trabaja con un tamanio de pagina de %d bytes!\n", pag_size);
	printf("Me conecte a MEMORIA en socket #%d\n",sock_mem);
	log_info(logTrace,"se trabaja con un tamanio de pagina de %d bytes",pag_size);
	//printf("Conectando con kernel...\n");
	log_trace(logTrace,"conectando con kernel");
	sock_kern = establecerConexion(cpu_data->ip_kernel, cpu_data->puerto_kernel);
	if (sock_kern < 0){
		log_error(logTrace,"fallo conexion a kernel");
		puts("Fallo conexion a Kernel");
		return FALLO_CONEXION;
	}

	if ((stat = handshakeCon(sock_kern, cpu_data->tipo_de_proceso)) < 0){
		log_error(logTrace,"no se pudo hacer handshake con kernel");
		fprintf(stderr, "No se pudo hacer hadshake con Kernel\n");
		return FALLO_GRAL;
	}
	log_trace(logTrace,"se enviaron %d bytes a kernel",stat);
	//printf("Se enviaron: %d bytes a KERNEL\n", stat);

	h_esp.tipo_de_proceso = KER; h_esp.tipo_de_mensaje = KERINFO;
	if ((stat = recibirInfo2ProcAProc(sock_kern, h_esp, &q_sleep, &stack_size)) != 0){
		log_error(logTrace,"no se pudo recibir el qsleep y stack size de kernel");
		puts("No se pudo recibir el quantum sleep y stack size de Kernel!");
		return ABORTO_CPU;
	}
	log_info(logTrace,"Quantum sleep: %d",q_sleep);
	log_info(logTrace,"Stack size: %d",stack_size);
	printf("Se trabaja con quant_sleep %d y stack_size %d\n", q_sleep, stack_size);
	set_quantum_sleep();
	log_trace(logTrace," me conecte con kernel");
	printf("Me conecte a kernel (socket %d)\n", sock_kern);

	return 0;
}
