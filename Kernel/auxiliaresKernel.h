#ifndef AUXILIARESKERNEL_H_
#define AUXILIARESKERNEL_H_

#include <tiposRecursos/tiposPaquetes.h>
#include <tiposRecursos/misc/pcb.h>


typedef struct {
	int pid;
	int sock;
	char* src_code;
	t_metadata_program meta;
}dataHiloProg; // todo: ver si es raro


typedef struct{
	int fd_cpu,pid,disponibilidad;
}t_cpu;

typedef struct{
	int fd_con,pid;
}t_consola;

/* Recibe, serializa y reenvia a Memoria el codigo fuente
 * El parametro src_size es un auxiliar para obtener ese dato hasta fuera del proceso
 */
int passSrcCodeFromRecv(tPackHeader *head, int fd_sender, int fd_mem, int *src_size);

/* Crea un puntero a un PCB nuevo, con un PID unico.
 */
tPCB *nuevoPCB(tPackSrcCode *src_code, int cant_pags, int sock_hilo);

/* Gestiona los mensaje recibidos del cpu que no sean para el planificador
 * */
void cpu_manejador(int sock);


#endif // AUXILIARESKERNEL_H_
