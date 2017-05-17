#ifndef TIPOSPAQUETES_H_
#define TIPOSPAQUETES_H_
#include <stdint.h>

#define HEAD_SIZE 8 // size de la cabecera de cualquier packete (tipo de proceso y de mensaje)

typedef enum {KER= 1, CPU= 2, FS= 3, CON= 4, MEM= 5} tProceso;

typedef enum {
	HSHAKE      = 1,
	SRC_CODE    = 3,
	// API Memoria
	ASIGN_PAG   = SRC_CODE,
	INI_PROG    = 4,
	FIN_PROG    = 5,
	SOLIC_BYTES = 6,
	ALMAC_BYTES = 7,
	FIN      = 11
} tMensaje;


typedef struct {

	tProceso tipo_de_proceso;
	tMensaje tipo_de_mensaje;
} tPackHeader; // este tipo de struct no necesita serialazion

typedef struct {

	tPackHeader head;
	unsigned long sourceLen;
	char *sourceCode;
} tPackSrcCode; // este paquete se utiliza para enviar codigo fuente

typedef struct {

	tPackHeader head;
	uint32_t pid;
	uint32_t pageCount;
} tPackPidPag; // este paquete se utiliza para enviar un pid y una cant de paginas

#endif /* TIPOSPAQUETES_H_ */
