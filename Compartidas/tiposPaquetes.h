#ifndef TIPOSPAQUETES_H_
#define TIPOSPAQUETES_H_
#include <stdint.h>

typedef enum {KER= 1, CPU= 2, FS= 3, CON= 4, MEM= 5} tProceso;

typedef enum {
	HSHAKE   = 1,
	SRC_CODE = 3,
	FIN      = 11
} tMensaje;


typedef struct {

	tProceso tipo_de_proceso;
	tMensaje tipo_de_mensaje;
} tPackHeader; // este tipo de struct no necesita serialazion


typedef struct {

	tPackHeader head;
	uint32_t sourceLen;
	void *sourceCode;
} tPackSrcCode;

#endif /* TIPOSPAQUETES_H_ */
