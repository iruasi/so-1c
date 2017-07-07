#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <commons/log.h>

#include <funcionesCompartidas/funcionesCompartidas.h>
#include <tiposRecursos/tiposPaquetes.h>

#include "auxiliaresConsola.h"
extern t_log logger;

/* Dado un archivo, lo lee e inserta en un paquete de codigo fuente
 */
tPackSrcCode *readFileIntoPack(tProceso sender, char* ruta){
// todo: en la estructura tPackSrcCode podriamos aprovechar mejor el espacio. Sin embargo, funciona bien

	FILE *file = fopen(ruta, "rb");
	tPackSrcCode *src_code = malloc(sizeof *src_code);
	src_code->head.tipo_de_proceso = sender;
	src_code->head.tipo_de_mensaje = SRC_CODE;

	unsigned long fileSize = fsize(file) + 1 ; // + 1 para el '\0'
	log_info(&logger,"fsize es: %lu","fileSize");
	//printf("fsize es %lu\n", fileSize);
	src_code->sourceLen = fileSize;
	src_code->sourceCode = malloc(src_code->sourceLen);
	fread(src_code->sourceCode, src_code->sourceLen, 1, file);
	fclose(file);
	// ponemos un '\0' al final porque es probablemente mandatorio para que se lea, send'ee y recv'ee bien despues
	src_code->sourceCode[src_code->sourceLen - 1] = '\0';

	return src_code;
}

void *serializarSrcCode(tPackSrcCode *src_code){

	int offset = 0;

	void *serial_src_code = malloc(sizeof src_code->head + sizeof src_code->sourceLen + src_code->sourceLen);
	if (serial_src_code == NULL){
		log_error(&logger,"no se puedo mallocar el serial_src_code");
		//perror("No se pudo mallocar el serial_src_code. error");
		return NULL;
	}

	memcpy(serial_src_code, &src_code->head, sizeof src_code->head);
	offset += sizeof src_code->head;

	memcpy(serial_src_code + offset, &src_code->sourceLen, sizeof src_code->sourceLen);
	offset += sizeof src_code->sourceLen;

	memcpy(serial_src_code + offset, src_code->sourceCode, src_code->sourceLen);

	return serial_src_code;
}

