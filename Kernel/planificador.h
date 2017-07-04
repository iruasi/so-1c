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


void setupPlanificador();
void encolarEnNewPrograma(tPCB *new_PCB, int sock_consola);
void updateQueue(t_queue *queue);

void cpu_handler_planificador(t_cpuInfo * cpu);

void freePCBs(t_queue *queue);
void limpiarPlanificadores(void);

#endif
