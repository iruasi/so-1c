#include <stdint.h>

#ifndef KERNEL_H_
#define KERNEL_H_


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
	uint32_t tipo_de_proceso;
}tKernel;

typedef struct _t_Package {
	uint32_t tipo_de_proceso;
	uint32_t tipo_de_mensaje;
	uint32_t archivo_size;
	void* archivo_a_enviar;
	uint32_t total_size;			// NOTA: Es calculable
} t_Package;


char* serializarOperandos(t_Package*);


/* Dada una ruta de acceso, crea una estructura configurada para Kernel;
 * retorna un puntero a la esctructura creada.
 */
tKernel *getConfigKernel(char *ruta);

/* Muestra en stdout la configuracion del Kernel
 */
void mostrarConfiguracion(tKernel *kernel);

/* Libera todos los recursos allocados de la estructura
 */
void liberarConfiguracionKernel(tKernel *kernel);

#endif /* KERNEL_H_ */

