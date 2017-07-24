#ifndef PLANIFICADOR_H_
#define PLANIFICADOR_H_

#include <parser/metadata_program.h>
#include <commons/collections/queue.h>

#include <tiposRecursos/misc/pcb.h>

#include "defsKernel.h"
#include "auxiliaresKernel.h"


#ifndef LOCK_PLANIF
#define LOCK_PLANIF (LOCK_MUX(&mux_new), LOCK_MUX(&mux_ready), LOCK_MUX(&mux_exec), LOCK_MUX(&mux_block), LOCK_MUX(&mux_exit))
#endif

#ifndef UNLOCK_PLANIF
#define UNLOCK_PLANIF (UNLOCK_MUX(&mux_new), UNLOCK_MUX(&mux_ready), UNLOCK_MUX(&mux_exec), UNLOCK_MUX(&mux_block), UNLOCK_MUX(&mux_exit))
#endif

void setupGlobales_planificador(void);

void setupPlanificador(void);
void setupSemaforosColas(void);

void mandarPCBaCPU(tPCB *pcb, t_RelCC * cpu);

void mergePCBs(tPCB **old, tPCB *new);

void planificar(void);

int finalizarEnMemoria(int pid);

void pausarPlanif(void);

int obtenerCPUociosa(void);

int getPCBPositionByPid(int pid, t_list *cola_pcb);

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

/* Entrelaza el CPU y el Programa, por medio del PID del PCB que comparten.
 * Se copian los valores de Programa en CPU y viceversa.
 */
void asociarProgramaACPU(t_RelCC *cpu);

void avisarPIDaPrograma(int pid, int sock_prog);
void iniciarYAlojarEnMemoria(t_RelPF *pf, int pages);

void blockByPID(int pid, tPCB *pcbCPU);
void unBlockByPID(int pid);

int fueFinalizadoPorConsola(int pid);

void encolarEnExit(tPCB *pcb,t_RelCC *cpu);
#endif
