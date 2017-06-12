#ifndef PLANIFICADOR_H_
#define PLANIFICADOR_H_

#include <parser/metadata_program.h>
#include <commons/collections/queue.h>

#include <tiposRecursos/misc/pcb.h>

typedef struct {
	int pid;
	int codeLen;
	char *srcCode;
	t_metadata_program *meta;
} tMetadataPCB;

void setupPlanificador();
void encolarEnNEWPrograma(tPCB *new_PCB, int sock_consola);
void updateQueue(t_queue *queue);

void freePCBs(t_queue *queue);
void limpiarPlanificadores(void);

#endif
