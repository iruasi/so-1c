#include<stdint.h>

#ifndef MEMORIACONFIGURATORS_H_
#define MEMORIACONFIGURATORS_H_

#define MAX_IP_LEN 16 // Este largo solo es util para IPv4
#define MAX_PORT_LEN 6



typedef struct{

	char* puerto_entrada;
	uint32_t marcos;
	uint32_t marco_size;
	uint32_t entradas_cache;
	uint32_t cache_x_proc;
	uint32_t retardo_memoria;
}tMemoria;

tMemoria *getConfigMemoria(char* ruta);
void mostrarConfiguracion(tMemoria *memoria);
void liberarConfiguracionMemoria(tMemoria *memoria);

#endif /* MEMORIACONFIGURATORS_H_ */
