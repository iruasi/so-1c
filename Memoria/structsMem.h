#ifndef STRUCTSMEM_H_
#define STRUCTSMEM_H_

#include <stdbool.h>
#include <stdint.h>

#define MAX_MESSAGE_SIZE 250


typedef struct{

	uint32_t size;
	bool     isFree;
} tHeapMeta;

typedef struct _t_PackageRecepcion {
	uint32_t tipo_de_proceso;
	uint32_t tipo_de_mensaje;
	char message[MAX_MESSAGE_SIZE];
	uint32_t message_long;			// NOTA: Es calculable. Aca lo tenemos por fines didacticos!
} t_PackageRecepcion;

typedef struct {

	uint32_t frame;
	uint32_t pid;
	uint32_t page;
} tCacheEntrada;

typedef struct {
	int pid;
	int pag;
} tEntradaInv;

int recieve_and_deserialize(t_PackageRecepcion *package, int socketCliente);


#endif /* STRUCTSMEM_H_ */
