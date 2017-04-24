#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <commons/config.h>

#include "../Compartidas/funcionesCompartidas.c"
#include "../Compartidas/tiposErrores.h"
#include "cpu.h"

#define MAX_IP_LEN 16 // Este largo solo es util para IPv4
#define MAX_PORT_LEN 6

#define MAXMSJ 100 // largo maximo de mensajes a enviar. Solo utilizado para 1er checkpoint

#define FALLO_GRAL -21
#define FALLO_CONFIGURACION -22


tCPU *getConfigCPU(char *ruta_archivo_configuracion);
void mostrarConfiguracionCPU(tCPU *datos_cpu);
void liberarConfiguracionCPU(tCPU *datos_cpu);


int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	char *buf = malloc(MAXMSJ);
	int i;
	int stat;
	int sock_kern, sock_mem;
	int bytes_sent;

	tCPU *cpu_data = getConfigCPU(argv[1]);
	mostrarConfiguracionCPU(cpu_data);

	// CONECTAR CON MEMORIA...
	printf("Conectando con memoria...\n");
	sock_mem = establecerConexion(cpu_data->ip_memoria, cpu_data->puerto_memoria);
	if (sock_mem < 0)
		return sock_mem;

	stat = recv(sock_mem, buf, MAXMSJ, 0);
	printf("%s\n", buf);
	clearBuffer(buf, MAXMSJ);

	if (stat < 0){
		printf("Error en la conexion con Memoria! status: %d \n", stat);
		return FALLO_CONEXION;
	}

	printf("socket de memoria es: %d\n",sock_mem);
	strcpy(buf, "Hola soy CPU");
	bytes_sent = send(sock_mem, buf, strlen(buf), 0);
	clearBuffer(buf, MAXMSJ);
	printf("Se enviaron: %d bytes a MEMORIA\n", bytes_sent);


	printf("Conectando con kernel...\n");
	sock_kern = establecerConexion(cpu_data->ip_kernel, cpu_data->puerto_kernel);
	if (sock_kern < 0)
		return sock_kern;

	while((stat = recv(sock_kern, buf, MAXMSJ, 0)) > 0){
		printf("%s\n", buf);

		clearBuffer(buf, MAXMSJ);
	}


	if (stat < 0){
		printf("Error en la conexion con Kernel! status: %d \n", stat);
		return FALLO_CONEXION;
	}


	printf("Kernel termino la conexion\nLimpiando proceso...\n");
	close(sock_kern);
	close(sock_mem);
	liberarConfiguracionCPU(cpu_data);
	return 0;
}



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

	config_destroy(cpu_conf);
	return cpu;
}

void mostrarConfiguracionCPU(tCPU *cpu){

	printf("IP kernel: %s\n",      cpu->ip_kernel);
	printf("Puerto kernel: %s\n",  cpu->puerto_kernel);
	printf("IP memoria: %s\n",     cpu->ip_memoria);
	printf("Puerto memoria: %s\n", cpu->puerto_memoria);
}

void liberarConfiguracionCPU(tCPU *cpu){
	free(cpu->ip_memoria);
	free(cpu->puerto_memoria);
	free(cpu->ip_kernel);
	free(cpu->puerto_kernel);
	free(cpu);
}
