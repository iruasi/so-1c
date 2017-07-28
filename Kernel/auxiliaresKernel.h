#ifndef AUXILIARESKERNEL_H_
#define AUXILIARESKERNEL_H_

#include <tiposRecursos/tiposPaquetes.h>
#include <tiposRecursos/misc/pcb.h>
#include <commons/collections/queue.h>

#include "capaMemoria.h"
#include "defsKernel.h"

typedef struct {
	int pid,
		cant_syscalls,
		rafagas_exec;
	infoHeap ih;
} t_infoProcess;

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

/*
 * OPERACIONES SOBRE TALBA GLOBAL Y TABLA POR PROCESO, DE ARCHIVOS
 */
tProcesoArchivo *crearArchivoDeProceso(int pid, tPackAbrir *abrir, tDatosTablaGlobal *dato);
tDatosTablaGlobal *agregarArchivoTablaGlobal(tPackAbrir *file);
void agregarArchivoATablaProcesos(tDatosTablaGlobal *datos,t_banderas flags, int pid);
tProcesoArchivo *obtenerProcesoSegunFDLocal(t_descriptor_archivo fd , int pid, char modo);
tProcesoArchivo * obtenerProcesoSegunFDGlobal(t_descriptor_archivo fd , int pid, char modo);
tDatosTablaGlobal * encontrarEnTablaGlobalPorFD(t_descriptor_archivo fd_local, int pid, char modo);
tDatosTablaGlobal * encontrarEnTablaGlobalporPath(char *path);
int crearArchivo(tPackAbrir *file, int sock_cpu, int sock_fs, int pid);
int validarArchivo(tPackAbrir *abrir, int sock_fs);
int cerrarArchivoDeProceso(int pid, int fd_local);

/*
 * OTRAS OPERACIONES
 */

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

int escribirAConsola(int pidProg,int sock_con, tPackRW *escr);

void crearInfoProcess(int pid);
t_infoProcess *getInfoProcessByPID(int pid);

int estaDesconectada(int pid);

void sumarSyscall(int pid);
void sumarPaginaHeap(int pid);
void sumarSizeYAlloc(int pid, int size);
void sumarFreeYDealloc(int pid, int size);

#endif // AUXILIARESKERNEL_H_

