/*
 * memoria.c
 *
 *  Created on: 13/4/2017
 *      Author: utnso
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include "memoria.h"
#include <commons/config.h>

#define MAX_IP_LEN 16 // Este largo solo es util para IPv4
#define MAX_PORT_LEN 6

#define MAXMSJ 100 // largo maximo de mensajes a enviar. Solo utilizado para 1er checkpoint

#define FALLO_GRAL -21
#define FALLO_CONFIGURACION -22

void mostrarConfiguracion(tMemoria *datos_memoria);
void liberarConfiguracionMemoria(tMemoria *datos_memoria);

int main(int argc, char* argv[]){

	//if(argc!=2){
	//		printf("Error en la cantidad de parametros\n");
	//		return EXIT_FAILURE;
	//	}

	//tMemoria *memoria = getConfigMemoria(argv[1]);
	int stat;
	int sock_kern;

	tMemoria *memoria = getConfigMemoria("config_memoria");

	mostrarConfiguracion(memoria);

	printf ("ip de kernel es:%s\npuerto kernel es:%s\n", memoria->ip_kernel, memoria->puerto_kernel);



}

tMemoria *getConfigMemoria(char* ruta){
	printf("Ruta del archivo de configuracion: %s\n", ruta);
	tMemoria *memoria = malloc(sizeof(tMemoria));

	memoria->ip_kernel  = malloc(MAX_IP_LEN * sizeof memoria->ip_kernel );
	memoria->puerto_kernel = malloc(MAX_PORT_LEN * sizeof memoria->puerto_kernel);
	memoria->puerto_propio = malloc(MAX_PORT_LEN * sizeof memoria->puerto_propio);

	t_config *memoriaConfig = config_create(ruta);

	strcpy(memoria->ip_kernel,      config_get_string_value(memoriaConfig, "IP_KERNEL"));
	strcpy(memoria->puerto_kernel,    config_get_string_value(memoriaConfig, "PUERTO_KERNEL"));
	strcpy(memoria->puerto_propio,    config_get_string_value(memoriaConfig, "PUERTO_PROPIO"));


	memoria->marcos = config_get_int_value(memoriaConfig, "MARCOS");
	memoria->marco_size = config_get_int_value(memoriaConfig, "MARCO_SIZE");
	memoria->entradas_cache = config_get_int_value(memoriaConfig, "ENTRADAS_CACHE");
	memoria->cache_x_proc = config_get_int_value(memoriaConfig, "CACHE_X_PROC");
	memoria->retardo_memoria = config_get_int_value(memoriaConfig, "RETARDO_MEMORIA");

	config_destroy(memoriaConfig);
	return memoria;


}

void mostrarConfiguracion(tMemoria *memoria){
	printf("IP para el kernel: %s\n", memoria->ip_kernel);
	printf("Puerto Kernel: %s\n", memoria->puerto_kernel);
	printf("Puerto Propio: %s\n", memoria->puerto_propio);
	printf("Marcos %d\n", memoria->marcos);
	printf("Marcosize: %d\n", memoria->marco_size);
	printf("Entradas cache: %d\n", memoria->entradas_cache);
	printf("Cache x proc: %d\n", memoria->cache_x_proc);
	printf("Retardo memoria: %d\n", memoria->retardo_memoria);
}

void liberarConfiguracionMemoria(tMemoria *memoria){

	free(memoria->ip_kernel);
	free(memoria->puerto_kernel);
	free(memoria->puerto_propio);
	free(memoria->marcos);
	free(memoria->marco_size);
	free(memoria->entradas_cache);
	free(memoria->cache_x_proc);
	free(memoria->retardo_memoria);

}



