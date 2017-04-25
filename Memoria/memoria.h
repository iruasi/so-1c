#include<stdint.h>

#ifndef MEMORIA_H_
#define MEMORIA_H_



#endif /* MEMORIA_H_ */

typedef struct{

	char* puerto_entrada;
	uint32_t marcos;
	uint32_t marco_size;
	uint32_t entradas_cache;
	uint32_t cache_x_proc;
	uint32_t retardo_memoria;
}tMemoria;

tMemoria *getConfigMemoria(char*);

