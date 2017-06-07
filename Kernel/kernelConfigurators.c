#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "kernelConfigurators.h"

#include <tiposRecursos/tiposPaquetes.h>

#include <commons/config.h>
#include <commons/string.h>

#define MAX_IP_LEN 16   // aaa.bbb.ccc.ddd -> son 15 caracteres, 16 contando un '\0'
#define MAX_PORT_LEN 6  // 65535 -> 5 digitos, 6 contando un '\0'
#define MAXMSJ 100
#define MAXALGO 4 		// cantidad maxima de carecteres para kernel->algoritmo (RR o FIFO)


tKernel *getConfigKernel(char* ruta){

	printf("Ruta del archivo de configuracion: %s\n", ruta);
	t_config *kernelConfig = config_create(ruta);

	tKernel *kernel = malloc(sizeof(tKernel));

	kernel->algoritmo   =    malloc(MAXALGO);
	kernel->ip_fs       =    malloc(MAX_IP_LEN);
	kernel->ip_memoria  =    malloc(MAX_IP_LEN);
	kernel->puerto_cpu  =    malloc(MAX_PORT_LEN);
	kernel->puerto_prog =    malloc(MAX_PORT_LEN);
	kernel->puerto_memoria = malloc(MAX_PORT_LEN);
	kernel->puerto_fs   =    malloc(MAX_PORT_LEN);

	strcpy(kernel->algoritmo,      config_get_string_value(kernelConfig, "ALGORITMO"));
	kernel->algo = (strcmp(kernel->algoritmo, "FIFO") == 0)? FIFO : RR;
	strcpy(kernel->ip_fs,          config_get_string_value(kernelConfig, "IP_FS"));
	strcpy(kernel->ip_memoria,     config_get_string_value(kernelConfig, "IP_MEMORIA"));
	strcpy(kernel->puerto_prog,    config_get_string_value(kernelConfig, "PUERTO_PROG"));
	strcpy(kernel->puerto_cpu,     config_get_string_value(kernelConfig, "PUERTO_CPU"));
	strcpy(kernel->puerto_memoria, config_get_string_value(kernelConfig, "PUERTO_MEMORIA"));
	strcpy(kernel->puerto_fs,      config_get_string_value(kernelConfig, "PUERTO_FS"));

	kernel->grado_multiprog = config_get_int_value(kernelConfig, "GRADO_MULTIPROG");
	kernel->quantum         = config_get_int_value(kernelConfig, "QUANTUM");
	kernel->quantum_sleep   = config_get_int_value(kernelConfig, "QUANTUM_SLEEP");
	kernel->stack_size      = config_get_int_value(kernelConfig, "STACK_SIZE");
	kernel->sem_ids         = config_get_array_value(kernelConfig, "SEM_IDS");
	kernel->sem_init        = config_get_array_value(kernelConfig, "SEM_INIT");
	kernel->shared_vars     = config_get_array_value(kernelConfig, "SHARED_VARS");
	kernel->tipo_de_proceso = KER;

	config_destroy(kernelConfig);
	return kernel;
}


// todo: hacer bien el recorrido de vectores
void mostrarConfiguracion(tKernel *kernel){

	printf("Algoritmo: %s\n", kernel->algoritmo);
	printf("Grado de multiprogramacion: %d\n", kernel->grado_multiprog);
	printf("IP para el File System: %s\n", kernel->ip_fs);
	printf("IP para la memoria: %s\n", kernel->ip_memoria);
	printf("Puerto CPU: %s\n", kernel->puerto_cpu);
	printf("Puerto para la memoria: %s\n", kernel->puerto_memoria);
	printf("Puerto para los programas: %s\n", kernel->puerto_prog);
	printf("Puerto para el filesystem: %s\n", kernel->puerto_fs);
	printf("Quantum: %d\n", kernel->quantum);
	printf("Quantum Sleep: %d\n", kernel->quantum_sleep);
	printf("Stack size: %d\n", kernel->stack_size);
	printf("Identificadores de semaforos: %s, %s, %s\n", kernel->sem_ids[0], kernel->sem_ids[1], kernel->sem_ids[2]);
	printf("Valores de semaforos: %s, %s, %s\n", kernel->sem_init[0], kernel->sem_init[1], kernel->sem_init[2]);
	printf("Variables globales: %s, %s, %s\n", kernel->shared_vars[0], kernel->shared_vars[1], kernel->shared_vars[2]);
	printf("Tipo de proceso: %d\n", kernel->tipo_de_proceso);
}

void liberarConfiguracionKernel(tKernel *kernel){

	free(kernel->algoritmo);
	free(kernel->ip_fs);
	free(kernel->ip_memoria);
	free(kernel->puerto_cpu);
	free(kernel->puerto_memoria);
	free(kernel->puerto_prog);
	free(kernel->puerto_fs);
	free(kernel);
}
