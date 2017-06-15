#include "funcionesAnsisop.h"
#include <tiposRecursos/tiposPaquetes.h>
#include <netdb.h>


//FUNCIONES DE ANSISOP
//TODO: error fantasma (? dice que hay multiple definicion de "termino" pero esta una vez sola definido.
t_puntero definirVariable(t_nombre_variable variable) {
	printf("definir la variable %c\n", variable);
	//list_add(pcb->indiceDeStack->vars, variable);
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

void irAlLabel (t_nombre_etiqueta t_nombre_etiqueta){
	printf("Se va al label %s\n", t_nombre_etiqueta);
	pcb->etiquetaSize = 4*sizeof(int32_t) + 2*sizeof(t_list); //TODO: ver esta formula, la vi por ahi
	pcb->pc = metadata_buscar_etiqueta(t_nombre_etiqueta, pcb->indiceDeEtiquetas, pcb->etiquetaSize);
}
void llamarSinRetorno (t_nombre_etiqueta etiqueta){
	printf("Se llama a la funcion %s\n", etiqueta);
	irAlLabel(etiqueta);
}

void llamarConRetorno (t_nombre_etiqueta etiqueta, t_puntero donde_retornar){
	printf("Se llama a la funcion %s y se guarda el retorno\n", etiqueta);
	//TODO: Hacer
}

void retornar (t_valor_variable retorno){
	//TODO: Hacer
}

t_valor_variable obtenerValorCompartida (t_nombre_compartida variable){
	printf("Se obtiene el valor de variable compartida.");
	return 20;
}



//FUNCIONES ANSISOP QUE LE PIDE AL KERNEL
void wait (t_nombre_semaforo identificador_semaforo){
	printf("Se pide al kernel un wait para el semaforo %s", identificador_semaforo);
	enviarAlKernel(S_WAIT);
}

void signal (t_nombre_semaforo identificador_semaforo){
	printf("Se pide al kernel un signal para el semaforo %s", identificador_semaforo);
	enviarAlKernel(S_SIGNAL);
}

void liberar (t_puntero puntero){
	printf("Se pide al kernel liberar memoria. Inicio: %d\n", puntero);
	enviarAlKernel(LIBERAR);
}

t_descriptor_archivo abrir (t_direccion_archivo direccion, t_banderas flags){
	printf("Se pide al kernel abrir el archivo %s\n", direccion);
	enviarAlKernel(ABRIR);
	return 10;
}

void borrar (t_descriptor_archivo direccion){
	printf("Se pide al kernel borrar el archivo %d\n", direccion);
	enviarAlKernel(BORRAR);
}

void cerrar (t_descriptor_archivo descriptor_archivo){
	printf("Se pide al kernel cerrar el archivo %d\n", descriptor_archivo);
	enviarAlKernel(CERRAR);
}

void moverCursor (t_descriptor_archivo descriptor_archivo, t_valor_variable posicion){
	printf("Se pide al kernel movel el archivo %d a la posicion %d\n", descriptor_archivo, posicion);
	enviarAlKernel(MOVERCURSOR);
}

void escribir (t_descriptor_archivo descriptor_archivo, void* informacion, t_valor_variable tamanio){
	printf("Se pide al kernel escribir el archivo %d con la informacion %s, cantidad de bytes: %d\n", descriptor_archivo, (char*)informacion, tamanio);
	enviarAlKernel(ESCRIBIR);
}

void leer (t_descriptor_archivo descriptor_archivo, t_puntero informacion, t_valor_variable tamanio){
	printf("Se pide al kernel leer el archivo %d, se guardara en %d, cantidad de bytes: %d\n", descriptor_archivo, informacion, tamanio);
	enviarAlKernel(LEER);
}

t_puntero reservar (t_valor_variable espacio){
	printf("Se pide al kernel reservar %d espacio de memoria", espacio);
	enviarAlKernel(RESERVAR);
	return 40;
}

void enviarAlKernel(tMensaje mensaje){
	tPackHeader h;
	h.tipo_de_proceso = CPU;
	h.tipo_de_mensaje = mensaje;
	send(sock_kern, &h, sizeof(h), 0);
}
