#include<stdint.h>

#ifndef KERNEL_H_
#define KERNEL_H_

#endif /* KERNEL_H_ */

#define FALLO_CONEXION -25

typedef struct{

	char* puerto_cpu;
	char* puerto_prog;
	char* puerto_memoria;
	char* puerto_fs;
	char* ip_memoria;
	char* ip_fs;
	uint8_t quantum;
	uint32_t quantum_sleep;
	char* algoritmo;
	uint8_t grado_multiprog;
	char** sem_ids;
	char** sem_init;
	char** shared_vars;
	uint8_t stack_size;
}tKernel;

tKernel *getConfigKernel(char*);
