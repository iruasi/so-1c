#include <stdint.h>

#ifndef MEMORIACONFIGURATORS_H_
#define MEMORIACONFIGURATORS_H_

#define MAX_IP_LEN 16 // Este largo solo es util para IPv4
#define MAX_PORT_LEN 6

typedef struct{

	char* puerto_entrada;
	int marcos,
		marco_size,
		entradas_cache,
		cache_x_proc,
		retardo_memoria,
		tipo_de_proceso;
}tMemoria;

tMemoria *getConfigMemoria(char* ruta);
void mostrarConfiguracion(tMemoria *memoria);
void liberarConfiguracionMemoria(void);

#endif /* MEMORIACONFIGURATORS_H_ */
