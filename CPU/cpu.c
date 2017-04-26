#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <commons/config.h>

#include "../Compartidas/funcionesCompartidas.c"
#include "../Compartidas/tiposErrores.h"
#include "cpuConfigurators.h"

#define MAX_IP_LEN 16 // Este largo solo es util para IPv4
#define MAX_PORT_LEN 6

#define MAXMSJ 100 // largo maximo de mensajes a enviar. Solo utilizado para 1er checkpoint



int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	char *buf = malloc(MAXMSJ);
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




