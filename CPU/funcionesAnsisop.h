#ifndef FUNCIONESANSISOP_H_
#define FUNCIONESANSISOP_H_

#include<stdbool.h>

#include <parser/parser.h>
#include <parser/metadata_program.h>

#include <tiposRecursos/misc/pcb.h>
#include <tiposRecursos/tiposPaquetes.h>

/*
 *
 * VARIABLES GLOBALES QUE UTILIZAN LAS FUNCIONES ANSISOP
 *
 */
bool termino;
tPCB* pcb;
int sock_mem; // SE PASA A VAR GLOBAL POR SIEMPRE
int sock_kern;
AnSISOP_funciones functions;
AnSISOP_kernel kernel_functions;

void setupCPUFunciones(void);
void setupCPUFuncionesKernel(void);

//FUNCIONES ANSISOP
t_puntero definirVariable(t_nombre_variable variable);
t_puntero obtenerPosicionVariable(t_nombre_variable variable);
void finalizar(void);
t_valor_variable dereferenciar(t_puntero puntero);
void asignar(t_puntero puntero, t_valor_variable variable);
t_valor_variable asignarValorCompartida(t_nombre_compartida variable, t_valor_variable valor);
void irAlLabel (t_nombre_etiqueta t_nombre_etiqueta);
void llamarSinRetorno (t_nombre_etiqueta etiqueta);
void llamarConRetorno (t_nombre_etiqueta etiqueta, t_puntero donde_retornar);
void retornar (t_valor_variable retorno);
t_valor_variable obtenerValorCompartida (t_nombre_compartida variable);


//FUNCIONES MODO KERNEL
void wait (t_nombre_semaforo identificador_semaforo);
void signal (t_nombre_semaforo identificador_semaforo);
void liberar (t_puntero puntero);
t_descriptor_archivo abrir (t_direccion_archivo direccion, t_banderas flags);
void borrar (t_descriptor_archivo direccion);
void cerrar (t_descriptor_archivo descriptor_archivo);
void moverCursor (t_descriptor_archivo descriptor_archivo, t_valor_variable posicion);
void escribir (t_descriptor_archivo descriptor_archivo, void* informacion, t_valor_variable tamanio);
void leer (t_descriptor_archivo descriptor_archivo, t_puntero informacion, t_valor_variable tamanio);
t_puntero reservar (t_valor_variable espacio);


void enviarAlKernel(tMensaje tipoMensaje);
#endif /* FUNCIONESANSISOP_H_ */
