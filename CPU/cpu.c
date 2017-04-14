#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>

#include <commons/config.h>
#include "cpu.h"

#define MAX_IP_LEN 16 // Este largo solo es util para IPv4
#define MAX_PORT_LEN 6

#define MAXMSJ 100 // largo maximo de mensajes a enviar. Solo utilizado para 1er checkpoint

#define FALLO_GRAL -21
#define FALLO_CONFIGURACION -22
#define FALLO_CONEXION -25

void setupHints(struct addrinfo *hints, int address_family, int socket_type, int flags);
int establecerConexion(char *ip_destino, char *puerto_destino);

tCPU *getConfigCPU(char *ruta_archivo_configuracion);
void mostrarConfiguracionCPU(tCPU *datos_cpu);
void liberarConfiguracionCPU(tCPU *datos_cpu);

int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	char buf[MAXMSJ];
	int i;
	int stat;
	int sock_kern, sock_mem;

	tCPU *cpu_data = getConfigCPU(argv[1]);
	if (cpu_data == NULL)
		return FALLO_CONFIGURACION;
	mostrarConfiguracionCPU(cpu_data);

	printf("Conectando con kernel...\n");
	sock_kern = establecerConexion(cpu_data->ip_kernel, cpu_data->puerto_kernel);
	if (sock_kern < 0)
			return sock_kern;

		while((stat = recv(sock_kern, buf, MAXMSJ, 0)) > 0){
			printf("%s\n", buf);

			for(i = 0; i < MAXMSJ; ++i)
				buf[i] = '\0';
		}

		if (stat == -1){
			printf("Error en la conexion con Kernel! status: %d \n", stat);
			return -1;
		}

		printf("Kernel termino la conexion\nLimpiando proceso...\n");

//	NO VAMOS A CONECTAR CON MEMORIA TODAVIA...
//	printf("Conectando con memoria...\n");
//	sock_mem = establecerConexion(ip_memoria, port_memoria);
//	printf("socket de memoria es: %d\n",sock_mem);

	close(sock_kern);
	close(sock_mem);
	liberarConfiguracionCPU(cpu_data);
	return 0;
}


void setupHints(struct addrinfo *hint, int fam, int sockType, int flags){

	memset(hint, 0, sizeof *hint);
	hint->ai_family = fam;
	hint->ai_socktype = sockType;
	hint->ai_flags = flags;
}



/* Dados un ip y puerto de destino, se crea, conecta y retorna socket apto para comunicacion
 */
int establecerConexion(char *ip_dest, char *port_dest){

	int stat;
	int sock_dest; // file descriptor para el socket del destino a conectar
	struct addrinfo hints, *destInfo;

	setupHints(&hints, AF_UNSPEC, SOCK_STREAM, 0);

	if ((stat = getaddrinfo(ip_dest, port_dest, &hints, &destInfo)) != 0){
		fprintf("getaddrinfo: %s\n", gai_strerror(stat));
		return FALLO_GRAL;
	}

	if ((sock_dest = socket(destInfo->ai_family, destInfo->ai_socktype, destInfo->ai_protocol)) < 0)
		return FALLO_GRAL;

	connect(sock_dest, destInfo->ai_addr, destInfo->ai_addrlen);
	freeaddrinfo(destInfo);

	if (sock_dest < 0){
		printf("Error al tratar de conectar con Kernel!");
		return FALLO_CONEXION;
	}

	return sock_dest;
}

tCPU *getConfigCPU(char *ruta_config){

	// Alojamos espacio en memoria para la esctructura que almacena los datos de config
	tCPU *cpu = malloc(sizeof *cpu);
	cpu->ip_memoria    = malloc(MAX_IP_LEN * sizeof cpu->ip_memoria);
	cpu->puerto_memoria= malloc(MAX_PORT_LEN * sizeof cpu->puerto_memoria);
	cpu->ip_kernel     = malloc(MAX_IP_LEN * sizeof cpu->ip_kernel);
	cpu->puerto_kernel = malloc(MAX_PORT_LEN * sizeof cpu->puerto_kernel);

	t_config *cpu_conf = config_create(ruta_config);

	if (!config_has_property(cpu_conf, "IP_MEMORIA") || !config_has_property(cpu_conf, "PUERTO_MEMORIA")){
		printf("No se detecto alguno de los parametros de configuracion!");
		return NULL;
	}

	if (!config_has_property(cpu_conf, "IP_KERNEL") || !config_has_property(cpu_conf, "PUERTO_KERNEL")){
		printf("No se detecto alguno de los parametros de configuracion!");
		return NULL;
	}

	strcpy(cpu->ip_memoria,     config_get_string_value(cpu_conf, "IP_MEMORIA"));
	strcpy(cpu->puerto_memoria, config_get_string_value(cpu_conf, "PUERTO_MEMORIA"));
	strcpy(cpu->ip_kernel,      config_get_string_value(cpu_conf, "IP_KERNEL"));
	strcpy(cpu->puerto_kernel,  config_get_string_value(cpu_conf, "PUERTO_KERNEL"));

	config_destroy(cpu_conf);
	return cpu;

}

void mostrarConfiguracionCPU(tCPU *cpu){

	printf("IP kernel: %s\n", cpu->ip_kernel);
	printf("Puerto kernel: %s\n", cpu->puerto_kernel);
	printf("IP memoria: %s\n", cpu->ip_memoria);
	printf("Puerto memoria: %s\n", cpu->puerto_memoria);
}

void liberarConfiguracionCPU(tCPU *cpu){
	free(cpu->ip_memoria);
	free(cpu->puerto_memoria);
	free(cpu->ip_kernel);
	free(cpu->puerto_kernel);
	free(cpu);
}
