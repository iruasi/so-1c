#ifndef AUXILIARESKERNEL_H_
#define AUXILIARESKERNEL_H_

#include <tiposRecursos/tiposPaquetes.h>
#include <tiposRecursos/misc/pcb.h>
#include <commons/collections/queue.h>


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


void liberarCC(t_RelCC *cc);

int getCPUPosByFD(int fd, t_list *list);


/* Crea un t_RelPF* y lo guarda en la lista global de Programas,
 * asi quedan asociados el socket de un Programa y su source code.
 */
void asociarSrcAProg(t_RelCC *con_i, tPackSrcCode *src);

/* Recibe, serializa y reenvia a Memoria el codigo fuente
 * El parametro src_size es un auxiliar para obtener ese dato hasta fuera del proceso
 */
int passSrcCodeFromRecv(tPackHeader *head, int fd_sender, int fd_mem, int *src_size);

void setupVariablesGlobales(void);

/* Crea un PCB que aun no tiene la info importante
 * que se obtiene a partir del meta y codigo fuente.
 * La info importante se obtiene y asigna en el pasaje de New->Ready.
 */
tPCB *crearPCBInicial(void);

/* Crea un puntero a un PCB nuevo, con un PID unico.
 */
tPCB *nuevoPCB(tPackSrcCode *src_code, int cant_pags, t_RelCC *prog);

/* Es un recv() constante al socket del hilo CPU.
 * Gestiona los mensaje recibidos del hilo.
 * Tiene que redireccionar a Planificador lo que corresponda.
 * El resto son en su mayoria Syscalls.
 */
void cpu_manejador(void *cpuInfo);

void mem_manejador(void *m_sock);

/* Es un recv() constante al socket del hilo Programa.
 * Gestiona los mensaje recibidos del hilo.
 */
void cons_manejador(void *conInfo);

int setGlobal(tPackValComp *val_comp);
t_valor_variable getGlobal(t_nombre_variable *var, bool *found);

void consolaKernel(void);

void mostrarColaDe (char* cola);

void mostrarInfoDe(int pidElegido);

void cambiarGradoMultiprogramacion(int nuevoGrado);

void finalizarProceso(int pidAFinalizar);

void stopKernel();

void mostrarTablaGlobal();

void mostrarColaNew();

void mostrarColaReady();

void mostrarColaExec();

void mostrarColaExit();

void mostrarColaBlock();

void mostrarCantRafagasEjecutadasDe(tPCB *pcb);

void mostrarCantPrivilegiadasDe(tPCB *pcb);

void mostrarTablaDeArchivosDe(tPCB *pcb);

void mostrarCantHeapUtilizadasDe(tPCB *pcb);

void mostrarCantSyscallsUtilizadasDe(tPCB *pcb);

void* queue_get(t_queue *self,int posicion);
char * serializeFileDescriptor(tPackFS * abrir,int *pack_size);
#endif // AUXILIARESKERNEL_H_
