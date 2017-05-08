#include <stdint.h>
#include "tiposHeader.h"

typedef struct {

	tProceso tipo_de_proceso;
	tMensaje tipo_de_mensaje;
} tPackHeader; // este tipo de struct no necesita serialazion

typedef struct {

	tPackHeader head;
	uint32_t sourceLen;
	void *sourceCode;
} tPackSrcCode;
