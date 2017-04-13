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


int setupConfig(t_config *configuracion, char *ip_kernel, char *ip_memoria, char *puerto_kernel, char *puerto_memoria);
int revisarYObtenerConf(t_config *configuracion, char *entrada_del_valor, char* valor_de_configuracion);

void setupHints(struct addrinfo *hints, int address_family, int socket_type, int flags);
int establecerConexion(char *ip_destino, char *puerto_destino);

tCPU *getConfigCPU(char *ruta_archivo_configuracion);
void liberarConfiguracionCPU(tCPU *datos_cpu);

int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	int stat;
	int sock_kern, sock_mem;

	tCPU *cpu_data = getConfigCPU(argv[1]);
	if (cpu_data == NULL)
		return FALLO_CONFIGURACION;

	printf ("ip de kernel es:%s\npuerto kernel es:%s\n", cpu_data->ip_kernel, cpu_data->puerto_kernel);
	printf ("ip de memoria es:%s\npuerto memoria es:%s\n", cpu_data->ip_memoria, cpu_data->puerto_memoria);

	printf("Conectando con kernel...\n");
	sock_kern = establecerConexion(cpu_data->ip_kernel, cpu_data->puerto_kernel);
	printf("socket de kernel es: %d\n",sock_kern);

//	printf("Conectando con memoria...\n");
//	sock_mem = establecerConexion(ip_memoria, port_memoria);
//	printf("socket de memoria es: %d\n",sock_mem);

	return 0;
}


void setupHints(struct addrinfo *hint, int fam, int sockType, int flags){

	memset(hint, 0, sizeof *hint);
	hint->ai_family = fam;
	hint->ai_socktype = sockType;
	hint->ai_flags = flags;
}


/*
 * Esta funcion conecta con un destino y envia un breve saludo.
 * Si falla retorna FALLO_GRAL; retorna el socket a destino si funciona bien.
 */
int establecerConexion(char *ip_dest, char *port_dest){

	char * msj = "Hola soy cpu!\0";

	int stat, bytes_sent;
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

	bytes_sent = send(sock_dest, msj, strlen(msj), 0);
	printf("Se enviaron: %d bytes\n", bytes_sent);


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

void liberarConfiguracionCPU(tCPU *cpu){
	free(cpu->ip_memoria);
	free(cpu->puerto_memoria);
	free(cpu->ip_kernel);
	free(cpu->puerto_kernel);
	free(cpu);
}
