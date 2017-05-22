#ifndef PLANIFICADOR_H_
#define PLANIFICADOR_H_

#include <commons/collections/queue.h>

#include "../Compartidas/pcb.h"

void startScheduling(int multiprog);

void encolarPrograma(tPCB *new_PCB, int sock_consola);
void freePCBs(t_queue *queue);
void limpiarPlanificadores(void);

#endif
