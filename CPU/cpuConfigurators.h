#ifndef CPUCONFIGURATORS_H_
#define CPUCONFIGURATORS_H_

#define MAX_IP_LEN 16 // Este largo solo es util para IPv4
#define MAX_PORT_LEN 6


#include <stdint.h>


typedef struct {
	char *ip_memoria;
	char *puerto_memoria;
	char *ip_kernel;
	char *puerto_kernel;
	uint32_t tipo_de_proceso;
} tCPU;

typedef struct _t_Package {
	uint32_t tipo_de_proceso;
	uint32_t tipo_de_mensaje;
	char* message;
	uint32_t message_long;
	uint32_t total_size;			// NOTA: Es calculable
} t_Package;

char* serializarOperandos(t_Package*);

tCPU *getConfigCPU(char *ruta_config);

void mostrarConfiguracionCPU(tCPU *cpu);

void liberarConfiguracionCPU(tCPU *cpu);

#endif /* CPUCONFIGURATORS_H_ */
