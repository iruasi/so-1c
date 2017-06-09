#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <stdbool.h>
#include <commons/config.h>

#include <funcionesCompartidas/funcionesCompartidas.h>
#include <funcionesPaquetes/funcionesPaquetes.h>
#include <tiposRecursos/tiposErrores.h>
#include <tiposRecursos/tiposPaquetes.h>
#include <tiposRecursos/misc/pcb.h>

#include "cpuConfigurators.h"

#include <parser/parser.h>
#include <parser/metadata_program.h>

#define MAX_IP_LEN 16 // Este largo solo es util para IPv4
#define MAX_PORT_LEN 6

#define MAXMSJ 100 // largo maximo de mensajes a enviar. Solo utilizado para 1er checkpoint

int sock_mem; // SE PASA A VAR GLOBAL POR AHORA
int sock_kern;

int cantidadTotalDeBytesRecibidos(int fdServidor, void * buffer, int tamanioBytes); // TODO: mover

int pedirInstruccion(tPCB *pcb);
int recibirInstruccion(char **linea, int instr_size);

int ejecutarPrograma(tPCB*);


char* conseguirDatosDeLaMemoria(char* , t_puntero_instruccion, t_size);

tPCB *recvPCB(void);

bool termino = false;

//FUNCIONES DE ANSISOP
t_puntero definirVariable(t_nombre_variable variable) {
	printf("definir la variable %c\n", variable);
	return 20;
}

t_puntero obtenerPosicionVariable(t_nombre_variable variable) {
	printf("Obtener posicion de %c\n", variable);
	return 20;
}

void finalizar(void){
	termino = true;
	printf("Finalizar\n");
}

t_valor_variable dereferenciar(t_puntero puntero) {
	printf("Dereferenciar %d y su valor es: %d\n", puntero, 20);
	return 20;
}

void asignar(t_puntero puntero, t_valor_variable variable) {
	printf("Asignando en %d el valor %d\n", puntero, variable);
}

t_valor_variable asignarValorCompartida(t_nombre_compartida variable, t_valor_variable valor){
	printf("Asignado en %s el valor %d\n", variable, valor);
	return valor;
}

void irAlLabel (t_nombre_etiqueta t_nombre_etiqueta){ // TODO: hacer. fseek?

}
void llamarSinRetorno (t_nombre_etiqueta etiqueta){
	printf("Se llama a la funcion %s\n", etiqueta);
	//TODO: faltaria llamar a la funcion.
}

void llamarConRetorno (t_nombre_etiqueta etiqueta, t_puntero donde_retornar){
	printf("Se llama a la funcion %s y se guarda el retorno\n", etiqueta);
	//TODO: idem sin retorno. Seria un donde_retornar=etiqueta()?
}

void retornar (t_valor_variable retorno){
	//TODO: Hacer
}

t_valor_variable obtenerValorCompartida (t_nombre_compartida variable){
	printf("Se obtiene el valor de variable compartida.");
	return 20;
}

AnSISOP_funciones functions = {
		.AnSISOP_definirVariable		= definirVariable,
		.AnSISOP_obtenerPosicionVariable= obtenerPosicionVariable,
		.AnSISOP_finalizar 				= finalizar,
		.AnSISOP_dereferenciar			= dereferenciar,
		.AnSISOP_asignar				= asignar,
		.AnSISOP_asignarValorCompartida = asignarValorCompartida,
		.AnSISOP_irAlLabel				= irAlLabel,
		.AnSISOP_llamarSinRetorno		= llamarSinRetorno,
		.AnSISOP_llamarConRetorno		= llamarConRetorno,
		.AnSISOP_retornar				= retornar,
		.AnSISOP_obtenerValorCompartida = obtenerValorCompartida,
};

//FUNCIONES ANSISOP QUE LE PIDE AL KERNEL
void wait (t_nombre_semaforo identificador_semaforo){
	printf("Se pide al kernel un wait para el semaforo %s", identificador_semaforo);
	tPackHeader h;
	h.tipo_de_proceso = CPU;
	h.tipo_de_mensaje = S_WAIT;
	send(sock_kern, &h, sizeof(h), 0);
}

void signal (t_nombre_semaforo identificador_semaforo){
	printf("Se pide al kernel un signal para el semaforo %s", identificador_semaforo);
	//TODO: send al kernel
}

void liberar (t_puntero puntero){
	printf("Se pide al kernel liberar memoria. Inicio: %d\n", puntero);
	//TODO: send al kernel
}

t_descriptor_archivo abrir (t_direccion_archivo direccion, t_banderas flags){
	printf("Se pide al kernel abrir el archivo %s\n", direccion);
	//TODO: send al kernel
	return 10;
}

void borrar (t_descriptor_archivo direccion){
	printf("Se pide al kernel borrar el archivo %d\n", direccion);
	//TODO: send al kernel
}

void cerrar (t_descriptor_archivo descriptor_archivo){
	printf("Se pide al kernel cerrar el archivo %d\n", descriptor_archivo);
	//TODO: send al kernel
}

void moverCursor (t_descriptor_archivo descriptor_archivo, t_valor_variable posicion){
	printf("Se pide al kernel movel el archivo %d a la posicion %d\n", descriptor_archivo, posicion);
	//TODO: send al kernel
}

void escribir (t_descriptor_archivo descriptor_archivo, void* informacion, t_valor_variable tamanio){
	printf("Se pide al kernel escribir el archivo %d con la informacion %s, cantidad de bytes: %d\n", descriptor_archivo, (char*)informacion, tamanio);
	//TODO: send al kernel
}

void leer (t_descriptor_archivo descriptor_archivo, t_puntero informacion, t_valor_variable tamanio){
	printf("Se pide al kernel leer el archivo %d, se guardara en %d, cantidad de bytes: %d\n", descriptor_archivo, informacion, tamanio);
	//TODO: send al kernel
}

t_puntero reservar (t_valor_variable espacio){
	printf("Se pide al kernel reservar %d espacio de memoria", espacio);
	//TODO: send al kernel
	return 40;
}

AnSISOP_kernel kernel_functions = {
		.AnSISOP_wait 					= wait,
		.AnSISOP_signal					= signal,
		.AnSISOP_abrir					= abrir,
		.AnSISOP_borrar					= borrar,
		.AnSISOP_cerrar					= cerrar,
		.AnSISOP_escribir				= escribir,
		.AnSISOP_leer					= leer,
		.AnSISOP_liberar				= liberar,
		.AnSISOP_moverCursor			= moverCursor,
		.AnSISOP_reservar				= reservar,
};


int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	int stat;

	tCPU *cpu_data = getConfigCPU(argv[1]);
	mostrarConfiguracionCPU(cpu_data);

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
	puts("Me conecte a kernel");


	tPackHeader *head = malloc(sizeof *head);
	tPCB *pcb;
	while((stat = recv(sock_kern, head, sizeof *head, 0)) > 0){
		puts("Se recibio un paquete de Kernel");

		if (head->tipo_de_mensaje == FIN){
			puts("Kernel se va!");
			liberarConfiguracionCPU(cpu_data);

		} else if (head->tipo_de_mensaje == PCB_EXEC){
			if((pcb = recvPCB()) == NULL){
				return FALLO_RECV;
			}

			puts("Recibimos un PCB para ejecutar...");
			if ((stat = ejecutarPrograma(pcb)) != 0){
				fprintf(stderr, "Fallo ejecucion de programa. status: %d\n", stat);
				puts("No se continua con la ejecucion del pcb");
			}


		} else {
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
	liberarConfiguracionCPU(cpu_data);
	return 0;
}

/* para el momento que ejecuta esta funcion, ya se recibio el HEADER de 8 bytes,
 * por lo tanto hay que recibir el resto del paquete...
 */
tPCB *recvPCB(void){

	tPCB *pcb = malloc(sizeof *pcb);

	//pcb->indiceDeCodigo    = malloc(sizeof pcb->indiceDeCodigo);
	//pcb->indiceDeStack     = malloc(sizeof pcb->indiceDeStack);
	//pcb->indiceDeEtiquetas = malloc(sizeof pcb->indiceDeEtiquetas);
	pcb->indiceDeCodigo->start = 0;
	pcb->indiceDeCodigo->offset = 4;
	//int sizeIndex; // TODO: se va a usar para recibir el size que ocupan los tres indices (por ahora comentados...)

	int stat;
	if((stat = recv(sock_kern, &pcb->id, sizeof pcb->id, 0)) == -1){
		perror("Fallo recepcion de PCB. error");
		return NULL;
	}
	if((stat = recv(sock_kern, &pcb->pc, sizeof pcb->pc, 0)) == -1){
		perror("Fallo recepcion de PCB. error");
		return NULL;
	}
	if((stat = recv(sock_kern, &pcb->paginasDeCodigo, sizeof pcb->paginasDeCodigo, 0)) == -1){
		perror("Fallo recepcion de PCB. error");
		return NULL;
	}
	if((stat = recv(sock_kern, &pcb->exitCode, sizeof pcb->exitCode, 0)) == -1){
		perror("Fallo recepcion de PCB. error");
		return NULL;
	}

	return pcb;
}

tPCB *deserializarPCB(char *pbc_serial, int pcb_bytes){

	int offset = 0;

	tPCB *pcb ;// TODO:= malloc ();


	return pcb;
}

int ejecutarPrograma(tPCB* pcb){

	int stat, instr_size;
	char *linea;

	puts("Empieza a ejecutar...");
	do{
		//LEE LA PROXIMA LINEA DEL PROGRAMA
		if ((stat = pedirInstruccion(pcb)) != 0){
			fprintf(stderr, "Fallo pedido de instruccion. stat: %d\n", stat);
			return FALLO_GRAL;
		}

		instr_size = pcb->indiceDeCodigo->start - pcb->indiceDeCodigo->offset;
		if ((stat = recibirInstruccion(&linea, instr_size)) != 0){
			fprintf(stderr, "Fallo recepcion de instruccion. stat: %d\n", stat);
			return FALLO_GRAL;
		}

		printf("La linea %d es: %s", (pcb->pc+1), linea);
		//ANALIZA LA LINEA LEIDA Y EJECUTA LA FUNCION ANSISOP CORRESPONDIENTE
		analizadorLinea(linea, &functions, &kernel_functions);
		freeAndNULL((void **) &linea);
		pcb->pc++;

	} while(!termino);

	puts("Termino de ejecutar...");
	termino = false;

	freeAndNULL((void **) &linea);
	return EXIT_SUCCESS;
}

int pedirInstruccion(tPCB *pcb){
	puts("Pide instruccion");

	int stat, pack_size;

	char *bytereq_serial = serializeByteRequest(pcb, &pack_size);

	if((stat = send(sock_mem, bytereq_serial, pack_size, 0)) == -1){
		perror("Fallo envio de paquete de pedido de bytes. error");
		return FALLO_SEND;
	}

	freeAndNULL((void **) &bytereq_serial);
	return 0;
}

int recibirInstruccion(char **linea, int instr_size){

	int stat;
	tPackHeader head;
	if ((*linea = realloc(*linea, instr_size)) == NULL){
		fprintf(stderr, "No se pudo reallocar %d bytes memoria para la siguiente linea de instruccion\n", instr_size);
		return FALLO_GRAL;
	}

	if ((stat = recv(sock_mem, &head, sizeof head, 0)) == -1){
		perror("Fallo recepcion de header. error");
		return FALLO_RECV;
	}

	if (head.tipo_de_proceso != MEM || head.tipo_de_mensaje != SEND_BYTES){
		fprintf(stderr, "Error de comunicacion. Se esperaban bytes de Memoria, pero se recibio de %d el mensaje %d\n",
				head.tipo_de_proceso, head.tipo_de_mensaje);
		return FALLO_GRAL;
	}

	if ((stat = recv(sock_mem, *linea, instr_size, 0)) == -1){
		perror("Fallo recepcion de instruccion. error");
		return FALLO_RECV;
	}

	return 0;
}

char *conseguirDatosDeLaMemoria(char *programa, t_puntero_instruccion inicioDeLaInstruccion, t_size tamanio) {
	char *aRetornar = calloc(1, 100);
	memcpy(aRetornar, programa + inicioDeLaInstruccion, tamanio);
	return aRetornar;
}

// TODO: mover a funcionesCompartidas.c y .h
int cantidadTotalDeBytesRecibidos(int fdServidor, void *buffer, int tamanioBytes) { //Esta función va en funcionesCompartidas
	int total = 0;
	int bytes_recibidos;

	while (total < tamanioBytes){

	bytes_recibidos = recv(fdServidor, buffer+total, tamanioBytes, MSG_WAITALL);
	// MSG_WAITALL: el recv queda completamente bloqueado hasta que el paquete sea recibido completamente

	if (bytes_recibidos == -1) { // Error al recibir mensaje
		perror("[SOCKETS] No se pudo recibir correctamente los datos.\n");
		break;
			}

	if (bytes_recibidos == 0) { // Conexión cerrada
		printf("[SOCKETS] La conexión fd #%d se ha cerrado.\n", fdServidor);
		break;
	}
	total += bytes_recibidos;
	tamanioBytes -= bytes_recibidos;
		}
	return bytes_recibidos; // En caso de éxito, se retorna la cantidad de bytes realmente recibida
}
