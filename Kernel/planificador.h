#ifndef PLANIFICADOR_H_
#define PLANIFICADOR_H_

#include <commons/collections/queue.h>

#include <tiposRecursos/misc/pcb.h>


void setupPlanificador();
void encolarEnNEWPrograma(tPCB *new_PCB, int sock_consola);
void updateQueue(t_queue *queue);

void freePCBs(t_queue *queue);
void limpiarPlanificadores(void);

#endif
