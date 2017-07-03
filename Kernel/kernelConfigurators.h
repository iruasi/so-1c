#ifndef KERNELCONFIGURATORS_H_
#define KERNELCONFIGURATORS_H_

#include <stdint.h>

#include <tiposRecursos/tiposPaquetes.h>

#define MAX_MESSAGE_SIZE 5000

typedef enum {FIFO = 1, RR = 2} tAlgoritmo;

typedef struct{

	char* puerto_cpu;
	char* puerto_prog;
	char* puerto_memoria;
	char* puerto_fs;
	char* ip_memoria;
	char* ip_fs;
	int quantum;
	uint32_t quantum_sleep;
	char *algoritmo;
	tAlgoritmo algo;
	int grado_multiprog;
	char** sem_ids;
	char** sem_init;
	int sem_quant;
	char** shared_vars;
	int shared_quant;
	int stack_size;
	tProceso tipo_de_proceso;
}tKernel;

/* Dada una ruta de acceso, crea una estructura configurada para Kernel;
 * retorna un puntero a la esctructura creada.
 */
tKernel *getConfigKernel(char *ruta);

/* Muestra en stdout la configuracion del Kernel
 */
void mostrarConfiguracion(tKernel *kernel);

void mostrarSemaforos(tKernel *kernel);

void mostrarGlobales(tKernel *kernel);

/* Libera todos los recursos allocados de la estructura
 */
void liberarConfiguracionKernel(tKernel *kernel);

#endif /* KERNELCONFIGURATORS_H_ */

