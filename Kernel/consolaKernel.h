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

void mostrarCantRafagasEjecutadasDe(int pid);

void mostrarTablaDeArchivosDe(int pid);

void mostrarTablaGlobalArchivos(void);

void mostrarCantHeapUtilizadasDe(int pid);

void mostrarCantSyscallsUtilizadasDe(int pid);

int getQueuePositionByPid(int pid, t_queue *queue);

void armarStringPermisos(char* permisos, int creacion, int lectura, int escritura);

#endif /* CONSOLA_KERNEL_H_ */
