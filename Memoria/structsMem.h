#ifndef STRUCTSMEM_H_
#define STRUCTSMEM_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct{

	uint32_t size;
	bool     isFree;
} tHeapMeta;

typedef struct {

	int pid;
	int page;
	char *dir_cont;
} tCacheEntrada;

typedef struct {
	int pid;
	int pag;
} tEntradaInv;

#endif /* STRUCTSMEM_H_ */
