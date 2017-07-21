#ifndef FUNCIONESANSISOP_H_
#define FUNCIONESANSISOP_H_

#include<stdbool.h>

#include <parser/parser.h>
#include <parser/metadata_program.h>

#include <tiposRecursos/misc/pcb.h>
#include <tiposRecursos/tiposPaquetes.h>


// macro para asignar offset, pagina y size a una variable de tipo posicionMemoria* (args)
#define SET_ARG_OPS(PMP, O, P, S) (O = (PMP)->offset, P = (PMP)->pag, S = (PMP)->size)

// macro para asignar offset, pagina y size a una variable de tipo posicionMemoria  (vars->pos)
#define SET_VAR_OPS( MP, O, P, S) (O = (MP).offset,   P = (MP).pag,   S = (MP).size)

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

/* Wrapper para send().
 * Lo utilizaria solamente la CPU para solicitar a Kernel
 * que ejecute una operacion privilegiada
 */
void enviar(char *op_kern_serial, int pack_size);

/* Retorna el puntero (relativo al primer stack) hasta el comienzo del ctxt */
t_puntero punteroAContexto(t_list *stack_ind, int ctxt);

/* Retorna la posicion de la ultima variable guardada en Stack*/
void obtenerUltimoEnContexto(t_list *stack, int *pag, int *off, int *size);

/* Retorna la posicion de una variable, en el contexto actual*/
void obtenerVariable(t_nombre_variable variable, posicionMemoria* pm, indiceStack* stack);

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
void signal_so (t_nombre_semaforo identificador_semaforo);
void liberar (t_puntero puntero);
t_descriptor_archivo abrir(t_direccion_archivo direccion,t_banderas flags);
void borrar (t_descriptor_archivo direccion);
void cerrar (t_descriptor_archivo descriptor_archivo);
void moverCursor (t_descriptor_archivo descriptor_archivo, t_valor_variable posicion);
void escribir (t_descriptor_archivo descriptor_archivo, void* informacion, t_valor_variable tamanio);
void leer (t_descriptor_archivo descriptor_archivo, t_puntero informacion, t_valor_variable tamanio);
t_puntero reservar (t_valor_variable espacio);


void enviarAlKernel(tMensaje tipoMensaje);
#endif /* FUNCIONESANSISOP_H_ */
