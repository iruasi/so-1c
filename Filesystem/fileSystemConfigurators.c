#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <commons/string.h>
#include <commons/config.h>
#include "fileSystemConfigurators.h"

#include <tiposRecursos/tiposPaquetes.h>

tFileSystem* getConfigFS(char* ruta){
	printf("Ruta del archivo de configuracion: %s\n", ruta);
	tFileSystem *fileSystem = malloc(sizeof(tFileSystem));

	fileSystem->puerto_entrada = malloc(MAX_PORT_LEN);
	fileSystem->punto_montaje  = string_new();
	fileSystem->ip_kernel      = malloc(MAX_IP_LEN);
	fileSystem->magic_num      = string_new();
	t_config *fileSystemConfig = config_create(ruta);

	strcpy(fileSystem->puerto_entrada,        config_get_string_value(fileSystemConfig, "PUERTO_ENTRADA"));
	string_append(&fileSystem->punto_montaje, config_get_string_value(fileSystemConfig, "PUNTO_MONTAJE"));
	strcpy(fileSystem->ip_kernel,             config_get_string_value(fileSystemConfig, "IP_KERNEL"));
	string_append(&fileSystem->magic_num,     config_get_string_value(fileSystemConfig, "MAGIC_NUMBER"));

	fileSystem->blk_size  = config_get_int_value(fileSystemConfig, "TAMANIO_BLOQUES");
	fileSystem->blk_quant = config_get_int_value(fileSystemConfig, "CANTIDAD_BLOQUES");

	fileSystem->tipo_de_proceso = FS;

	config_destroy(fileSystemConfig);
	return fileSystem;
}

void mostrarConfiguracion(tFileSystem *fileSystem){

	printf("Puerto: %s\n",              fileSystem->puerto_entrada);
	printf("Punto de montaje: %s\n",    fileSystem->punto_montaje);
	printf("IP del kernel: %s\n",       fileSystem->ip_kernel);
	printf("Size de bloques: %d\n",     fileSystem->blk_size);
	printf("Cantidad de bloques: %d\n", fileSystem->blk_quant);
	printf("Magic number: %s\n",        fileSystem->magic_num);

}

void liberarConfiguracionFileSystem(tFileSystem *fileSystem){

	free(fileSystem->punto_montaje);
	free(fileSystem->ip_kernel);
	free(fileSystem->puerto_entrada);
	free(fileSystem->magic_num);
	free(fileSystem);
}

