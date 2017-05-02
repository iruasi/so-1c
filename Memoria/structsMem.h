#ifndef STRUCTSMEM_H_
#define STRUCTSMEM_H_

#include <stdbool.h>

#define MAX_MESSAGE_SIZE 250


typedef struct{

	uint32_t size;
	bool     isFree;
} tHeapMeta;

typedef struct _t_Package {
	uint32_t tipo_de_proceso;
	uint32_t tipo_de_mensaje;
	char message[MAX_MESSAGE_SIZE];
	uint32_t message_long;			// NOTA: Es calculable. Aca lo tenemos por fines didacticos!
} t_Package;

typedef struct{

	uint32_t frame;
	uint32_t pid;
	uint32_t page;
} tMemEntrada;


#endif /* STRUCTSMEM_H_ */
