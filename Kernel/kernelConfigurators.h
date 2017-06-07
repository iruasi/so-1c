#include <stdint.h>

#ifndef KERNELCONFIGURATORS_H_
#define KERNELCONFIGURATORS_H_

#define MAX_MESSAGE_SIZE 5000

typedef enum {FIFO = 1, RR = 2} tAlgoritmo;

typedef struct{

	char* puerto_cpu;
	char* puerto_prog;
	char* puerto_memoria;
	char* puerto_fs;
	char* ip_memoria;
	char* ip_fs;
	uint8_t quantum;
	uint32_t quantum_sleep;
	char *algoritmo;
	tAlgoritmo algo;
	uint8_t grado_multiprog;
	char** sem_ids;
	char** sem_init;
	char** shared_vars;
	uint8_t stack_size;
	uint32_t tipo_de_proceso;
}tKernel;

typedef struct _t_PackageEnvio {
	uint32_t tipo_de_proceso;
	uint32_t tipo_de_mensaje;
	uint32_t message_size;
	char* message;
	uint32_t total_size;			// NOTA: Es calculable
} t_PackageEnvio;

typedef struct _t_PackageRecepcion {
	uint32_t tipo_de_proceso;
	uint32_t tipo_de_mensaje;
	char message[MAX_MESSAGE_SIZE];
	uint32_t message_size;			// NOTA: Es calculable. Aca lo tenemos por fines didacticos!
} t_PackageRecepcion;


char* serializarOperandos(t_PackageEnvio*);
int recieve_and_deserialize(t_PackageRecepcion *package, int socketCliente);


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

#endif /* KERNELCONFIGURATORS_H_ */

