#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../Compartidas/funcionesCompartidas.h"
#include "../Compartidas/tiposPaquetes.h"
#include "auxiliaresConsola.h"

/* Dado un archivo, lo lee e inserta en un paquete de codigo fuente
 */
tPackSrcCode *readFileIntoPack(tProceso sender, char* ruta){
// todo: en la estructura tPackSrcCode podriamos aprovechar mejor el espacio. Sin embargo, funciona bien

	printf("\n%sa\n",ruta);
	FILE *file = fopen(ruta, "rb");
	puts("1");
	tPackSrcCode *src_code = malloc(sizeof *src_code);
	puts("2");
	src_code->head.tipo_de_proceso = sender;
	puts("3");
	src_code->head.tipo_de_mensaje = SRC_CODE;
	puts("4");
	unsigned long fileSize = fsize(file) + 1 ; // + 1 para el '\0'
	puts("5");
	printf("fsize es %lu\n", fileSize);
	puts("6");
	src_code->sourceLen = fileSize;
	puts("7");
	src_code->sourceCode = malloc(src_code->sourceLen);
	puts("8");
	fread(src_code->sourceCode, src_code->sourceLen, 1, file);
	puts("9");
	fclose(file);
	// ponemos un '\0' al final porque es probablemente mandatorio para que se lea, send'ee y recv'ee bien despues
	src_code->sourceCode[src_code->sourceLen - 1] = '\0';

	return src_code;
}

void *serializarSrcCode(tPackSrcCode *src_code){

	int offset = 0;

	void *serial_src_code = malloc(sizeof src_code->head + sizeof src_code->sourceLen + src_code->sourceLen);
	if (serial_src_code == NULL){
		perror("No se pudo mallocar el serial_src_code. error");
		return NULL;
	}

	memcpy(serial_src_code, &src_code->head, sizeof src_code->head);
	offset += sizeof src_code->head;

	memcpy(serial_src_code + offset, &src_code->sourceLen, sizeof src_code->sourceLen);
	offset += sizeof src_code->sourceLen;

	puts("esto vamos a meter en serializado");
	puts(src_code->sourceCode);

	memcpy(serial_src_code + offset, src_code->sourceCode, src_code->sourceLen);

	return serial_src_code;
}

