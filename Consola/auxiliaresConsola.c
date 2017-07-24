#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <commons/log.h>

#include <funcionesCompartidas/funcionesCompartidas.h>
#include <tiposRecursos/tiposPaquetes.h>

#include "auxiliaresConsola.h"
extern t_log *logger;

/* Dado un archivo, lo lee e inserta en un paquete de codigo fuente
 */
tPackSrcCode *readFileIntoPack(tProceso sender, char* ruta){

	FILE *file = fopen(ruta, "rb");
	tPackSrcCode *src_code = malloc(sizeof *src_code);
	src_code->head.tipo_de_proceso = sender;
	src_code->head.tipo_de_mensaje = SRC_CODE;

	unsigned long fileSize = fsize(file) + 1 ; // + 1 para el '\0'
	log_info(logger,"fsize es: %lu",fileSize);
	printf("fsize es %lu\n", fileSize);
	src_code->bytelen = fileSize;
	src_code->bytes = malloc(src_code->bytelen);
	fread(src_code->bytes, src_code->bytelen, 1, file);
	fclose(file);
	// ponemos un '\0' al final porque es probablemente mandatorio para que se lea, send'ee y recv'ee bien despues
	src_code->bytes[src_code->bytelen - 1] = '\0';

	return src_code;
}
