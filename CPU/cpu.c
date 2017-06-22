#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <stdbool.h>

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

#define MAX_IP_LEN 16 // Este largo solo es util para IPv4
#define MAX_PORT_LEN 6

#define MAXMSJ 100 // largo maximo de mensajes a enviar. Solo utilizado para 1er checkpoint
tPCB *deserializarPCB(char *pcb_serial);
char *recvGeneric(int sock_in);
tPCB *nuevoPCB(tPackSrcCode *src_code, int cant_pags, int sock_hilo);

int pedirInstruccion(int instr_size);
int recibirInstruccion(char **linea, int instr_size);

int ejecutarPrograma(void);

char* conseguirDatosDeLaMemoria(char* , t_puntero_instruccion, t_size);

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
	printf("Me conecte a kernel (socket %d)\n", sock_kern);


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

			pcb = nuevoPCB(NULL, 3, 3);

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

	int stat, instr_size;
	char **linea = malloc(0);
	*linea = NULL;

	termino = false;

	puts("Empieza a ejecutar...");
	do{
		instr_size = pcb->indiceDeCodigo->offset;

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
		pcb->pc++;
		pcb->indiceDeCodigo++;


	} while(!termino);
	freeAndNULL((void **) linea);

	puts("Termino de ejecutar...");
	termino = false;

	freeAndNULL((void **) &linea);
	return EXIT_SUCCESS;
}

int pedirInstruccion(int instr_size){
	puts("Pide instruccion");

	int stat, pack_size;

	char *bytereq_serial = serializeByteRequest(pcb, instr_size, &pack_size);

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

char *conseguirDatosDeLaMemoria(char *programa, t_puntero_instruccion inicioDeLaInstruccion, t_size tamanio) {
	char *aRetornar = calloc(1, 100);
	memcpy(aRetornar, programa + inicioDeLaInstruccion, tamanio);
	return aRetornar;
}

tPCB *deserializarPCB(char *pcb_serial){
	puts("Deserializamos PCB");

	int offset = 0;
	size_t indiceCod_size;
	tPCB *pcb;

	if ((pcb = malloc(sizeof *pcb)) == NULL){
		fprintf(stderr, "Fallo malloc\n");
		return NULL;
	}

	memcpy(&pcb->id, pcb_serial + offset, sizeof(int));
	offset += sizeof(int);
	memcpy(&pcb->pc, pcb_serial + offset, sizeof(int));
	offset += sizeof(int);
	memcpy(&pcb->paginasDeCodigo, pcb_serial + offset, sizeof(int));
	offset += sizeof(int);
	memcpy(&pcb->etiquetaSize, pcb_serial + offset, sizeof(int));
	offset += sizeof(int);
	memcpy(&pcb->cantidad_instrucciones, pcb_serial + offset, sizeof(int));
	offset += sizeof(int);
	memcpy(&pcb->estado_proc, pcb_serial + offset, sizeof(int));
	offset += sizeof(int);
	memcpy(&pcb->contextoActual, pcb_serial + offset, sizeof(int));
	offset += sizeof(int);
	memcpy(&pcb->exitCode, pcb_serial + offset, sizeof(int));
	offset += sizeof(int);

	indiceCod_size = pcb->cantidad_instrucciones * 2 * sizeof(int);
	if ((pcb->indiceDeCodigo = malloc(indiceCod_size)) == NULL){
		fprintf(stderr, "Fallo malloc\n");
		return NULL;
	}

	memcpy(pcb->indiceDeCodigo, pcb_serial + offset, indiceCod_size);
	offset += indiceCod_size;

	deserializarStack(pcb, pcb_serial, &offset);

	// si etiquetaSize es 0, malloc() retorna un puntero equivalente a NULL
	pcb->indiceDeEtiquetas = malloc(pcb->etiquetaSize);
	if (pcb->etiquetaSize){ // si hay etiquetas, las memcpy'amos
		memcpy(pcb->indiceDeEtiquetas, pcb_serial + offset, pcb->etiquetaSize);
		offset += pcb->etiquetaSize;
	}

	return pcb;
}

char *recvGeneric(int sock_in){
	puts("Se recibe el paquete serializado..");

	int stat, pack_size;
	char *p_serial;

	if ((stat = recv(sock_in, &pack_size, sizeof(int), 0)) <= 0){
		perror("Fallo de recv. error");
		return NULL;
	}

	printf("Paquete de size: %d\n", pack_size);

	if ((p_serial = malloc(pack_size)) == NULL){
		printf("No se pudieron mallocar %d bytes para paquete generico\n", pack_size);
		return NULL;
	}

	if ((stat = recv(sock_in, p_serial, pack_size, 0)) <= 0){
		perror("Fallo de recv. error");
		return NULL;
	}

	return p_serial;
}


tPCB *nuevoPCB(tPackSrcCode *src_code, int cant_pags, int sock_hilo){

	FILE * f = fopen("/home/utnso/CPU/facil.ansisop", "rb");
	char *src_cod = malloc(72);
	fread(src_cod, 72, 1, f);

	t_metadata_program *meta = metadata_desde_literal(src_code->sourceCode);
	t_size indiceCod_size = meta->instrucciones_size * 2 * sizeof(int);

	bool hayEtiquetas = (meta->etiquetas_size > 0)? true : false;

	tPCB *nuevoPCB = malloc(sizeof *nuevoPCB);
	nuevoPCB->indiceDeCodigo = malloc(indiceCod_size);

	nuevoPCB->indiceDeStack = list_create();

	nuevoPCB->etiquetaSize = 0;
	if (hayEtiquetas){
		nuevoPCB->etiquetaSize = meta->etiquetas_size;
		nuevoPCB->indiceDeEtiquetas = malloc(nuevoPCB->etiquetaSize);
		memcpy(nuevoPCB->indiceDeEtiquetas, meta->etiquetas, nuevoPCB->etiquetaSize);
	}

	nuevoPCB->id = 0;
	nuevoPCB->pc = 0;
	nuevoPCB->paginasDeCodigo = cant_pags;
	nuevoPCB->estado_proc = 0;
	nuevoPCB->contextoActual = 0; // todo: verificar que esta inicializacion sea coherente
	nuevoPCB->cantidad_instrucciones = meta->instrucciones_size;
	memcpy(nuevoPCB->indiceDeCodigo, meta->instrucciones_serializado, indiceCod_size);

	nuevoPCB->indiceDeEtiquetas = meta->etiquetas;

	nuevoPCB->exitCode = 0;

	//almacenar(nuevoPCB->id, meta);

	return nuevoPCB;
}
