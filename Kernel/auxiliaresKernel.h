#ifndef AUXILIARESKERNEL_H_
#define AUXILIARESKERNEL_H_

#include <tiposRecursos/tiposPaquetes.h>
#include <tiposRecursos/misc/pcb.h>
#include <commons/collections/queue.h>

#include "capaMemoria.h"

typedef struct{
	int fd_cpu,pid;
}t_cpu;

typedef struct{
	int fd_con,pid;
}t_consola;

typedef struct {
	t_cpu     cpu;
	t_consola *con;
	tMensaje msj;
} t_RelCC; // Relacion Consola <---> CPU

typedef struct {
	t_RelCC *prog;
	tPackSrcCode *src;
} t_RelPF; // Relacion Programa <---> Codigo Fuente

typedef struct {
	int pid,ecode;
}t_finConsola;

typedef struct {
	int pid,
		cant_syscalls;
	infoHeap *ih;
} t_infoProcess;

typedef struct{
	t_direccion_archivo direccion;
	int cantidadOpen;
	t_descriptor_archivo fd;
}tDatosTablaGlobal;

typedef struct{
	t_banderas flag;
	t_descriptor_archivo fd;
	t_valor_variable posicionCursor;
}tProcesoArchivo;

typedef struct{
	int pid;
	t_list * archivosPorProceso; //Esta lista va a contener tProcesoArchivo
}t_procesoXarchivo;



void cpu_manejador(void *cpuInfo);
void mem_manejador(void *m_sock);
void cons_manejador(void *conInfo);

int *formarPtrPIDs(int *len);

void liberarCC(t_RelCC *cc);

int getConPosByFD(int fd, t_list *list);

int getCPUPosByFD(int fd, t_list *list);

int getInfoProcPosByPID(t_list *infoProc, int pid);

/* Crea un t_RelPF* y lo guarda en la lista global de Programas,
 * asi quedan asociados el socket de un Programa y su source code.
 */
void asociarSrcAProg(t_RelCC *con_i, tPackSrcCode *src);

/* Recibe, serializa y reenvia a Memoria el codigo fuente
 * El parametro src_size es un auxiliar para obtener ese dato hasta fuera del proceso
 */
int passSrcCodeFromRecv(tPackHeader *head, int fd_sender, int fd_mem, int *src_size);

void setupGlobales_auxiliares(void);

void agregarArchivoTablaGlobal(tDatosTablaGlobal * datos,tPackAbrir * abrir);
void agregarArchivoATablaProcesos(tDatosTablaGlobal *datos,t_banderas flags, int pid);

tProcesoArchivo * obtenerProcesoSegunFD(t_descriptor_archivo fd , int pid);

tDatosTablaGlobal * encontrarTablaPorFD(t_descriptor_archivo fd, int pid);


/* Crea un PCB que aun no tiene la info importante
 * que se obtiene a partir del meta y codigo fuente.
 * La info importante se obtiene y asigna en el pasaje de New->Ready.
 */
tPCB *crearPCBInicial(void);

/* Crea un puntero a un PCB nuevo, con un PID unico.
 */
tPCB *nuevoPCB(tPackSrcCode *src_code, int cant_pags, t_RelCC *prog);

int setGlobal(tPackValComp *val_comp);
t_valor_variable getGlobal(t_nombre_variable *var, bool *found);

void* queue_get(t_queue *self, int posicion);

void desconexionCpu(t_RelCC *cpu_i);

bool estaEnExit(int pid);

#endif // AUXILIARESKERNEL_H_
