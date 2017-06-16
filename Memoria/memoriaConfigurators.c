#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <commons/config.h>
#include "memoriaConfigurators.h"
#include <tiposRecursos/tiposPaquetes.h>
#include <funcionesCompartidas/funcionesCompartidas.h>

extern tMemoria *memoria;

tMemoria *getConfigMemoria(char* ruta){
	printf("Ruta del archivo de configuracion: %s\n", ruta);
	tMemoria *memoria = malloc(sizeof(tMemoria));

	memoria->puerto_entrada = malloc(MAX_PORT_LEN);

	t_config *memoriaConfig = config_create(ruta);

	strcpy(memoria->puerto_entrada, config_get_string_value(memoriaConfig, "PUERTO_ENTRADA"));

	memoria->marcos =          config_get_int_value(memoriaConfig, "MARCOS");
	memoria->marco_size =      config_get_int_value(memoriaConfig, "MARCO_SIZE");
	memoria->entradas_cache =  config_get_int_value(memoriaConfig, "ENTRADAS_CACHE");
	memoria->cache_x_proc =    config_get_int_value(memoriaConfig, "CACHE_X_PROC");
	memoria->retardo_memoria = config_get_int_value(memoriaConfig, "RETARDO_MEMORIA");
	memoria->tipo_de_proceso = MEM;

	config_destroy(memoriaConfig);
	return memoria;
}

void mostrarConfiguracion(tMemoria *memoria){

	printf("Puerto Entrada: %s\n",  memoria->puerto_entrada);
	printf("Marcos %d\n",           memoria->marcos);
	printf("Marcosize: %d\n",       memoria->marco_size);
	printf("Entradas cache: %d\n",  memoria->entradas_cache);
	printf("Cache x proc: %d\n",    memoria->cache_x_proc);
	printf("Retardo memoria: %d\n", memoria->retardo_memoria);
	printf("Tipo de proceso: %d\n", memoria->tipo_de_proceso);
}

void liberarConfiguracionMemoria(void){

	freeAndNULL((void **) &memoria->puerto_entrada);
	freeAndNULL((void **) &memoria);
}
