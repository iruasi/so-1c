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



int pedirInstruccion(tPCB *pcb, char **linea);
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
		puts("Se recibio un paquete de Kernel");

		if (head->tipo_de_mensaje == FIN){
			puts("Kernel se va!");
			liberarConfiguracionCPU(cpu_data);

		} else if (head->tipo_de_mensaje == PCB_EXEC){
			if((pcb = recvPCB()) == NULL){
				return FALLO_RECV;
			}

			puts("Recibimos un PCB para ejecutar...");
			ejecutarPrograma(pcb);


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

tPCB *recvPCB(){

	tPCB *pcb = malloc(sizeof *pcb);
	int sizeIndex; // TODO: se va a usar para recibir el size que ocupan los tres indices (por ahora comentados...)
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


int ejecutarPrograma(tPCB* pcb){

	int stat;
	char *linea;

	puts("Empieza a ejecutar...");
	do{
		//LEE LA PROXIMA LINEA DEL PROGRAMA
		if ((stat = pedirInstruccion(pcb, &linea)) != 0)
			return FALLO_GRAL;

		printf("La linea %d es: %s", (pcb->pc+1), linea);
		//ANALIZA LA LINEA LEIDA Y EJECUTA LA FUNCION ANSISOP CORRESPONDIENTE
		analizadorLinea(linea, &functions, &kernel_functions);
		freeAndNULL(linea);
		pcb->pc++;
	} while(!termino);

	puts("Termino de ejecutar...");
	termino = false;

	return EXIT_SUCCESS;
}

int pedirInstruccion(tPCB *pcb, char **linea){

	tPackSrcCode *line_code;

	tPackByteReq *pbrq = malloc(sizeof *pbrq);
	pbrq->head.tipo_de_proceso = CPU;
	pbrq->head.tipo_de_mensaje = INSTRUC_GET;
	pbrq->pid  = pcb->id;
	pbrq->page = 0;
	pbrq->offset = pcb->indiceDeCodigo->offsetInicio;
	pbrq->size = pcb->indiceDeCodigo->offsetFin - pcb->indiceDeCodigo->offsetInicio;
	send(sock_mem, pbrq, sizeof (tPackByteReq), 0);

	line_code = deserializeSrcCode(sock_kern);
	*linea = malloc(line_code->sourceLen);
	memcpy(*linea, line_code->sourceCode, line_code->sourceLen);

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
