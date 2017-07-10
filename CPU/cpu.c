#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>

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

int pedirInstruccion(int instr_size);
int recibirInstruccion(char **linea, int instr_size);

int ejecutarPrograma(void);
void set_quantum_sleep(void);
int conectarConServidores(tCPU *cpu_data);

int pag_size;
int q_sleep;
float q_sleep_segs;

int exec_err;
sem_t sem_fallo_exec;

void set_quantum_sleep(void){
	q_sleep_segs = q_sleep / 1000.0;
	printf("Se cambio la latencia de ejecucion de instruccion a %f segundos\n", q_sleep_segs);
}

/*
 * TODO: hacer un hilo que chequee regularmente que no se haya roto la ejecucion del PCB
 * Si algo se rompe, manda el pid, exitCode y un header a Planificador.
 */
void err_handler(void){
while(1){

	sem_wait(&sem_fallo_exec);

	int pcb_exitCode_mock = exec_err;
	switch(exec_err){

	case FALLO_CONEXION: // por ejemplo...
		puts("Se detecto FALLO_CONEXION! \nDeteniendo ejecucion...");
		send(sock_kern, &pcb_exitCode_mock, sizeof(int), 0);
		break;

	default:
		break;
	}

}} // el While va a permanecer por tanto tiempo como exista el hilo main()...

int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	int stat;

	tCPU *cpu_data = getConfigCPU(argv[1]);
	mostrarConfiguracionCPU(cpu_data);

	setupCPUFunciones();
	setupCPUFuncionesKernel();

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

			pcb = deserializarPCB(pcb_serial);

			puts("Recibimos un PCB para ejecutar...");
			if ((stat = ejecutarPrograma()) != 0){
				fprintf(stderr, "Fallo ejecucion de programa. status: %d\n", stat);
				puts("No se continua con la ejecucion del pcb");
			}

		} else {
			puts("Me re fui");
			return -99;
		}

		//todo: aca vendria otro if cuando kernel tiene q imprimir algo por consola. le manda un mensaje.
	}


	if (stat == -1){
		perror("Error en la recepcion con Kernel. error");
		return FALLO_RECV;
	}


	printf("Kernel termino la conexion\nLimpiando proceso...\n");
	close(sock_kern);
	close(sock_mem);
	free(pcb);
	liberarConfiguracionCPU(cpu_data);
	return 0;
}


int ejecutarPrograma(void){
	sleep(2); // todo: esto esta por race condition vs Kernel para escribir/leer a Memoria las instrucciones
	int stat;
	t_size instr_size;
	tPackHeader header;
	char **linea = malloc(0);
	*linea = NULL;
	bool fin_quantum = false;
	termino = false;


	int cantidadDeRafagas = 0;

	puts("Empieza a ejecutar...");

	do{
		instr_size = (pcb->indiceDeCodigo + pcb->pc)->offset;

		//LEE LA PROXIMA LINEA DEL PROGRAMA
		if ((stat = pedirInstruccion(instr_size)) != 0){
			fprintf(stderr, "Fallo pedido de instruccion. stat: %d\n", stat);
			return FALLO_GRAL;
		}

		if ((stat = recibirInstruccion(linea, instr_size)) != 0){
			fprintf(stderr, "Fallo recepcion de instruccion. stat: %d\n", stat);
			return FALLO_GRAL;
		}

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

	puts("Termino de ejecutar...");

	if (pcb->pc == pcb->cantidad_instrucciones){ // el PCB ejecuto la ultima instruccion todo: revisar logica de esto
		header.tipo_de_mensaje = FIN_PROCESO;
		pcb->exitCode = 100; // exit_success
	}

	else if(fin_quantum == true) // se dealoja el PCB, faltandole ejecutar instrucciones
		header.tipo_de_mensaje = PCB_PREEMPT;

	else
		header.tipo_de_mensaje = ABORTO_PROCESO;

	header.tipo_de_proceso = CPU;

	int pack_size = 0;
	char * pcb_serial = serializePCB(pcb, header, &pack_size); // todo: Necesitamos retornar el PCB completo siempre?
	printf("Se serializo bien el pcb con tamanio: %d\n", pack_size);
	if ((stat = send(sock_kern, pcb_serial, pack_size, 0)) == -1)
		perror("Fallo envio de PCB a Kernel. error");

	printf("Se enviaron %d bytes a kernel \n", stat);

	free(linea);
	free(pcb_serial);
	return EXIT_SUCCESS;
}

int pedirInstruccion(int instr_size){
	puts("Pide instruccion");

	tPackByteReq pbrq;
	int stat, pack_size, code_page, offset;
	code_page = (pcb->indiceDeCodigo + pcb->pc)->start / pag_size;
	offset    = (pcb->indiceDeCodigo + pcb->pc)->start % pag_size;

	pbrq.head.tipo_de_proceso = MEM; pbrq.head.tipo_de_mensaje = BYTES;
	pbrq.pid    = pcb->id;
	pbrq.page   = code_page;
	pbrq.offset = offset;
	pbrq.size   = instr_size;

	pack_size = 0;
	char *bytereq_serial = serializeByteRequest(&pbrq, &pack_size);

	if((stat = send(sock_mem, bytereq_serial, pack_size, 0)) == -1){
		perror("Fallo envio de paquete de pedido de bytes. error");
		return FALLO_SEND;
	}

	freeAndNULL((void **) &bytereq_serial);
	return 0;
}

int recibirInstruccion(char **linea, int instr_size){
	puts("vamos a recibir instruccion");

	int stat;
	char *byte_serial;
	tPackHeader head;
	tPackBytes *instr;

	if ((*linea = realloc(*linea, instr_size)) == NULL){
		fprintf(stderr, "No se pudo reallocar %d bytes memoria para la siguiente linea de instruccion\n", instr_size);
		return FALLO_GRAL;
	}

	if ((stat = recv(sock_mem, &head, HEAD_SIZE, 0)) == -1){
		perror("Fallo recepcion de header. error");
		return FALLO_RECV;
	}

	if (head.tipo_de_proceso != MEM || head.tipo_de_mensaje != BYTES){
		fprintf(stderr, "Error de comunicacion. Se esperaban bytes de Memoria, pero se recibio de %d el mensaje %d\n",
				head.tipo_de_proceso, head.tipo_de_mensaje);
		return FALLO_GRAL;
	}

	if ((byte_serial = recvGeneric(sock_mem)) == NULL){
		puts("Fallo recepcion de bytes");
		return FALLO_RECV;
	}

	if ((instr = deserializeBytes(byte_serial)) == NULL){
		puts("Fallo deserializacion de bytes");
		return FALLO_DESERIALIZAC;
	}
	memcpy(*linea, instr->bytes, instr->bytelen);

	return 0;
}


int conectarConServidores(tCPU *cpu_data){
	int stat;

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

	if ((stat = recibirInfoCPUMem(sock_mem, &pag_size)) != 0){
		puts("No se pudo recibir el tamanio de pagina de Memoria!");
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

	if ((stat = recibirInfoCPUKernel(sock_kern, &q_sleep)) != 0){
		puts("No se pudo recibir el quantum sleep de Kernel!");
		return ABORTO_CPU;
	}
	set_quantum_sleep();
	printf("Me conecte a kernel (socket %d)\n", sock_kern);

	return 0;
}


