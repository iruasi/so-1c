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


int pedirInstruccion(tPCB *pcb);
int recibirInstruccion(char *linea, int instr_size);

int ejecutarPrograma(tPCB*);
void ejecutarAsignacion();
void obtenerDireccion(variable*); // TODO: ADAPTAR
uint32_t obtenerValor(uint32_t, uint32_t, uint32_t);
void almacenar(uint32_t,uint32_t,uint32_t,uint32_t);
uint32_t getPagDelIndiceStack(uint32_t);
uint32_t getOffsetDelIndiceStack(uint32_t);
uint32_t getSizeDelIndiceStack(uint32_t);


char* conseguirDatosDeLaMemoria(char* , t_puntero_instruccion, t_size);

tPCB *recvPCB();

bool termino = false;

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

AnSISOP_funciones functions = {
		.AnSISOP_definirVariable		= definirVariable,
		.AnSISOP_obtenerPosicionVariable= obtenerPosicionVariable,
		.AnSISOP_finalizar 				= finalizar,
		.AnSISOP_dereferenciar			= dereferenciar,
		.AnSISOP_asignar				= asignar,

};
AnSISOP_kernel kernel_functions = { };


int sock_mem; // SE PASA A VAR GLOBAL POR AHORA
int sock_kern;

int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	char *buf = malloc(MAXMSJ);
	int stat;
	int bytes_sent;

	tCPU *cpu_data = getConfigCPU(argv[1]);
	mostrarConfiguracionCPU(cpu_data);


	strcpy(buf, "Hola soy CPU");

	// CONECTAR CON MEMORIA...
	printf("Conectando con memoria...\n");
	sock_mem = establecerConexion(cpu_data->ip_memoria, cpu_data->puerto_memoria);
	if (sock_mem < 0){
		puts("Fallo conexion a Memoria");
		return FALLO_CONEXION;
	}

	printf("Conectando con kernel...\n");
	sock_kern = establecerConexion(cpu_data->ip_kernel, cpu_data->puerto_kernel);
	if (sock_kern < 0){
		puts("Fallo conexion a Kernel");
		return FALLO_CONEXION;
	}


	tPackHeader *head;
	tPCB *pcb;
	while((stat = recv(sock_kern, head, sizeof *head, 0)) > 0){

		if (head->tipo_de_mensaje == FIN){
			puts("Kernel se va!");
			liberarConfiguracionCPU(cpu_data);

		} else if (head->tipo_de_mensaje == PCB_EXEC){
			if((pcb = recvPCB()) == NULL){
				return FALLO_RECV;
			}

			ejecutarPrograma(pcb);


		} else {
			return -99;
		}
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
 * Hace malloc del PCB, no olvidarse de hacerle free() en algun otro lado
 */
tPCB *recvPCB(void){

	tPCB *pcb;
	if ((pcb = malloc(sizeof *pcb)) == NULL){
		fprintf(stderr, "No se pudo crear espacio de memoria para pcb\n");
		return NULL;
	}

	if ((pcb->indiceDeCodigo = malloc(sizeof pcb->indiceDeCodigo)) == NULL){
		fprintf(stderr, "No se pudo crear espacio de memoria para pcb->indiceDeCodigo\n");
		return NULL;
	}

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


	// hardcodeamos esto por ahora porque no los recv()
	pcb->indiceDeCodigo->offsetInicio = 0;
	pcb->indiceDeCodigo->offsetFin = 4;

	return pcb;
}


int ejecutarPrograma(tPCB* pcb){

	int stat, instr_size;
	char *linea; // TODO: recibirInstruccion(linea) no esta funcionando bien, revisar dereferenciacion

	do{
		//LEE LA PROXIMA LINEA DEL PROGRAMA
		if ((stat = pedirInstruccion(pcb)) != 0)
			return FALLO_GRAL;

		instr_size = pcb->indiceDeCodigo->offsetFin - pcb->indiceDeCodigo->offsetInicio;
		if ((stat = recibirInstruccion(linea, instr_size)) != 0){
			fprintf(stderr, "Fallo recepcion de instruccion. stat: %d\n", stat);
			return FALLO_GRAL;
		}

		printf("La linea %d es: %s", (pcb->pc+1), linea);
		//ANALIZA LA LINEA LEIDA Y EJECUTA LA FUNCION ANSISOP CORRESPONDIENTE
		analizadorLinea(linea, &functions, &kernel_functions);
		freeAndNULL(linea);
		pcb->pc++;
	} while(!termino);

	termino = false;

	return EXIT_SUCCESS;
}

int pedirInstruccion(tPCB *pcb){
	puts("Pide instruccion");

	int stat, pack_size;

	puts("Serializa byte request");
	char *bytereq_serial = serializeByteRequest(pcb, &pack_size);

	puts("Envia byte request");
	if((stat = send(sock_mem, bytereq_serial, pack_size, 0)) == -1){
		perror("Fallo envio de paquete de pedido de bytes. error");
		return FALLO_SEND;
	}

	puts("Libera byte request");
	freeAndNULL((void **) &bytereq_serial);
	return 0;
}

int recibirInstruccion(char *linea, int instr_size){

	int stat;
	tPackHeader head;
	if ((linea = realloc(linea, instr_size)) == NULL){
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

	if ((stat = recv(sock_mem, linea, instr_size, 0)) == -1){
		perror("Fallo recepcion de instruccion. error");
		return FALLO_RECV;
	}


	return 0;
}

void ejecutarAsignacion(){
	variable* var = malloc(sizeof(variable));
	obtenerDireccion(var);


	/*
	 * POR AHORA SE HARDCODEA PARA PROBAR UN EJEMPLO DE UNA SUMA SIMPLE
	 */

	var->valor = obtenerValor(var->pag, var->offset, var->size);
	variable* varRes = malloc(sizeof(variable));
	obtenerDireccion(varRes);
	varRes->valor = var->valor+3;
	printf("El resultado de la suma es: %d\n", varRes->valor);
	almacenar(varRes->pag, varRes->offset, varRes->size, varRes->valor);

}

void obtenerDireccion(variable* var){
	var->pag = getPagDelIndiceStack(var->id);
	var->offset = getOffsetDelIndiceStack(var->id);
	var->size = getSizeDelIndiceStack(var->id);
}
uint32_t obtenerValor(uint32_t pag, uint32_t offset, uint32_t size){
	/*char* buf = malloc(MAXMSJ);
	char* recibido = malloc(MAXMSJ);
	sprintf(buf, "Pido valor de variable, pag: %d, offset:%d, size: %d", pag, offset, size);
	send(sock_mem, buf, strlen(buf), 0);
	recv(sock_mem, recibido, MAXMSJ, 0);
	int valor = atoi(recibido);
	return valor;
	*/
	/*
	 * HABRIA QUE DEFINIR EL PASO DE MENSAJES ENTRE MEMORIA Y CPU
	 * PARA ESTE EJEMPLO SE VA A PASAR UN VALOR PARA PROBAR QUE FUNCIONE
	 */
	return 5;
}

void almacenar(uint32_t pag, uint32_t off, uint32_t size, uint32_t valor){
	char* buf = malloc(MAXMSJ);
	sprintf(buf, "Envio valor de variable, pag: %d, offset: %d, size: %d, valor: %d", pag, off, size, valor);
	send(sock_mem, buf, strlen(buf), 0);
	free(buf);
}
/*
 * ESTAS FUNCIONES QUE VIENEN SE HACEN PARA QUE FUNCIONE, CREO QUE LE TIENE QUE
 * PEDIR A MEMORIA ESTOS VALORES
 *
 */
uint32_t getPagDelIndiceStack(uint32_t id){
	if(id==1) return 1;
	return 2;
}

uint32_t getOffsetDelIndiceStack(uint32_t id){
	if(id==1) return 1;
	return 2;
}

uint32_t getSizeDelIndiceStack(uint32_t id){
	if(id==1) return 1;
	return 2;
}
char* serializarOperandos(t_Package *package){

	char *serializedPackage = malloc(package->total_size);
	int offset = 0;
	int size_to_send;


	size_to_send =  sizeof(package->tipo_de_proceso);
	memcpy(serializedPackage + offset, &(package->tipo_de_proceso), size_to_send);
	offset += size_to_send;


	size_to_send =  sizeof(package->tipo_de_mensaje);
	memcpy(serializedPackage + offset, &(package->tipo_de_mensaje), size_to_send);
	offset += size_to_send;

	size_to_send =  sizeof(package->message_long);
	memcpy(serializedPackage + offset, &(package->message_long), size_to_send);
	offset += size_to_send;

	size_to_send =  package->message_long;
	memcpy(serializedPackage + offset, package->message, size_to_send);

	return serializedPackage;
}

char * conseguirDatosDeLaMemoria(char *programa, t_puntero_instruccion inicioDeLaInstruccion, t_size tamanio) {
	char *aRetornar = calloc(1, 100);
	memcpy(aRetornar, programa + inicioDeLaInstruccion, tamanio);
	return aRetornar;
}
