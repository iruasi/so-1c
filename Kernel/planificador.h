#ifndef PLANIFICADOR_H_
#define PLANIFICADOR_H_

#include <parser/metadata_program.h>
#include <commons/collections/queue.h>

#include <tiposRecursos/misc/pcb.h>

#include "auxiliaresKernel.h"

#define DISPONIBLE 1
#define OCUPADO 2


typedef struct {
	int pid;
	int codeLen;
	char *srcCode;
	t_metadata_program *meta;
} tMetadataPCB;


/* SEMAFOROS */
pthread_mutex_t mux_new, mux_ready, mux_exec, mux_block, mux_exit;
pthread_mutex_t mux_listaDeCPU;

void setupPlanificador(void);
void setupSemaforosColas(void);


void encolarEnNewPrograma(tPCB *new_PCB, int sock_consola);

/* Encolar en New no tiene gran efecto,
 * por ahora es un metodo sencillo que se encarga de pushear.
 */
void encolarEnNew(tPCB *pcb);

/* El pcb no tiene metadata, hay que crearlo y setearlo, ademas le avisamos
 * al hilo del Programa de su PID y a la Memoria que inicialice las paginas.
 * Hecho esto podemos colocar el PCB bien armado en la cola de Ready.
 * Caso contrario, solo setea el PCB con los valores que correspondan.
 */
void encolarDeNewEnReady(tPCB *pcb);

void updateQueue(t_queue *queue);

void cpu_handler_planificador(t_RelCC *cpu);

void freePCBs(t_queue *queue);
void limpiarPlanificadores(void);


/* FUNCIONES EXTRA HECHAS AL VUELO */ // todo: ver si las delegamos en otros archivos
t_RelPF *getProgByPID(int pid);

void asociarProgramaACPU(t_RelCC *cpu);
void avisarPIDaPrograma(int pid, int sock_prog);
void iniciarYAlojarEnMemoria(t_RelPF *pf, int pages);

#endif
