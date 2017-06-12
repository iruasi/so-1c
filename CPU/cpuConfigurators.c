#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <commons/config.h>
#include "cpuConfigurators.h"

#include <tiposRecursos/tiposPaquetes.h>

extern t_log * logger;

tCPU *getConfigCPU(char *ruta_config){

	// Alojamos espacio en memoria para la esctructura que almacena los datos de config
	tCPU *cpu = malloc(sizeof *cpu);
	cpu->ip_memoria    = malloc(MAX_IP_LEN);
	cpu->puerto_memoria= malloc(MAX_PORT_LEN);
	cpu->ip_kernel     = malloc(MAX_IP_LEN);
	cpu->puerto_kernel = malloc(MAX_PORT_LEN);

	t_config *cpu_conf = config_create(ruta_config);

	strcpy(cpu->ip_memoria,     config_get_string_value(cpu_conf, "IP_MEMORIA"));
	strcpy(cpu->puerto_memoria, config_get_string_value(cpu_conf, "PUERTO_MEMORIA"));
	strcpy(cpu->ip_kernel,      config_get_string_value(cpu_conf, "IP_KERNEL"));
	strcpy(cpu->puerto_kernel,  config_get_string_value(cpu_conf, "PUERTO_KERNEL"));

	cpu->tipo_de_proceso = CPU;

	config_destroy(cpu_conf);
	return cpu;
}

void mostrarConfiguracionCPU(tCPU *cpu){

	printf("IP kernel: %s\n",      cpu->ip_kernel);
	printf("Puerto kernel: %s\n",  cpu->puerto_kernel);
	printf("IP memoria: %s\n",     cpu->ip_memoria);
	printf("Puerto memoria: %s\n", cpu->puerto_memoria);
	printf("Tipo de proceso: %d\n", cpu->tipo_de_proceso);
}

void liberarConfiguracionCPU(tCPU *cpu){
	free(cpu->ip_memoria);
	free(cpu->puerto_memoria);
	free(cpu->ip_kernel);
	free(cpu->puerto_kernel);
	free(cpu);
}

