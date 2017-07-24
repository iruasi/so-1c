#ifndef CONSOLA_KERNEL_H_
#define CONSOLA_KERNEL_H_

void setupGlobales_consolaKernel(void);

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

int getQueuePositionByPid(int pid, t_queue *queue);

void armarStringPermisos(char* permisos, int creacion, int lectura, int escritura);

#endif /* CONSOLA_KERNEL_H_ */
