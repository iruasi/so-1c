#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <commons/log.h>
#include <commons/config.h>

#include "fileSystemConfigurators.h"

#include <tiposRecursos/tiposPaquetes.h>

extern t_log * logger;

tFileSystem* getConfigFS(char* ruta){
	log_info(logger,"Ruta del archivo de configuracion: %s\n", ruta);
	tFileSystem *fileSystem= malloc(sizeof(tFileSystem));

	fileSystem->puerto_entrada = malloc(MAX_PORT_LEN);
	fileSystem->punto_montaje  = malloc(MAX_IP_LEN);
	fileSystem->ip_kernel      = malloc(MAX_IP_LEN);

	t_config *fileSystemConfig = config_create(ruta);

	strcpy(fileSystem->puerto_entrada, config_get_string_value(fileSystemConfig, "PUERTO_ENTRADA"));
	strcpy(fileSystem->punto_montaje,  config_get_string_value(fileSystemConfig, "PUNTO_MONTAJE"));
	strcpy(fileSystem->ip_kernel,      config_get_string_value(fileSystemConfig, "IP_KERNEL"));
	fileSystem->tipo_de_proceso = FS;

	config_destroy(fileSystemConfig);
	return fileSystem;
}

void mostrarConfiguracion(tFileSystem *fileSystem){

	printf("Puerto: %s\n",           fileSystem->puerto_entrada);
	printf("Punto de montaje: %s\n", fileSystem->punto_montaje);
	printf("IP del kernel: %s\n",    fileSystem->ip_kernel);
	printf("Tipo de proceso: %d\n",    fileSystem->tipo_de_proceso);
}

void liberarConfiguracionFileSystem(tFileSystem *fileSystem){

	free(fileSystem->punto_montaje);
	free(fileSystem->ip_kernel);
	free(fileSystem->puerto_entrada);
	free(fileSystem);
}

