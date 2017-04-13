/*
 Estos includes los saque de la guia Beej... puede que ni siquiera los precisemos,
 pero los dejo aca para futuro, si algo mas complejo no anda tal vez sirvan...

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>

#include <commons/config.h>
#include "consola.h"

#define MAX_IP_LEN 16
#define MAX_PORT_LEN 6
#define MAXMSJ 100

#define FALLO_GRAL -21
#define FALLO_CONFIGURACION -22

tConsola *getConfigConsola(char *ruta_archivo_configuracion);
void liberarConfiguracionConsola(tConsola *datos_consola);


int setupConfig(t_config *configuracion, char *ip_kernel, char *port_kernel);
void setupHints(struct addrinfo *hints, int address_family, int socket_type, int flags);
int establecerConexion(char *ip_kernel, char *port_kernel);

void Iniciar_Programa();
void Finalizar_Programa(int process_id);
void Desconectar_Consola();
void Limpiar_Mensajes();


int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	tConsola *cons_data = getConfigConsola(argv[1]);
	if (cons_data == NULL)
		return FALLO_CONFIGURACION;

	printf ("ip es: %s\npuerto es: %s\n", cons_data->ip_kernel, cons_data->puerto_kernel);

	int sock = establecerConexion(cons_data->ip_kernel, cons_data->puerto_kernel);
	printf("El socket usado fue %d", sock);

	liberarConfiguracionConsola(cons_data);
	return 0;
}


int setupConfig(t_config* cfg, char *ip_kernel, char *port_kernel){

	if (!config_has_property(cfg, "IP_KERNEL")){
			printf("No se detecto el valor IP_KERNEL!");
			return FALLO_CONFIGURACION;
	}
	strcpy(ip_kernel, config_get_string_value(cfg, "IP_KERNEL"));

	if (!config_has_property(cfg, "PUERTO_KERNEL")){
		printf("No se detecto el valor PUERTO_KERNEL!");
		return FALLO_CONFIGURACION;
	}
	strcpy(port_kernel, config_get_string_value(cfg, "PUERTO_KERNEL"));

	return 0;
}


void setupHints(struct addrinfo *hint, int fam, int sockType, int flags){

	memset(hint, 0, sizeof *hint);
	hint->ai_family = fam;
	hint->ai_socktype = sockType;
	hint->ai_flags = flags;
}

/* Esta funcion conecta con kernel y retorna un file descriptor del socket a kernel
 * La deberia utilizar unicamente Iniciar_Programa, por cada nuevo hilo para un script que se crea
 */
int establecerConexion(char *ip_kernel, char *port_kernel){

	char * msj = "Hola! Soy un script en consola!";
	int bytes_sent;

	int stat;
	int sock_kern; // file descriptor para el socket del kernel
	struct addrinfo hints, *serverInfo;

	setupHints(&hints, AF_UNSPEC, SOCK_STREAM, 0);

	if ((stat = getaddrinfo(ip_kernel, port_kernel, &hints, &serverInfo)) != 0){
		fprintf("getaddrinfo: %s\n", gai_strerror(stat));
		return FALLO_GRAL;
	}

	sock_kern = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol);

	connect(sock_kern, serverInfo->ai_addr, serverInfo->ai_addrlen);
	freeaddrinfo(serverInfo);

	// send retorna un int, pero no nos interesa para error checking aun
	bytes_sent = send(sock_kern, msj, strlen(msj), 0);
	printf("Se enviaron: %d bytes\n", bytes_sent);

	return sock_kern;
}

/* Carga los datos de configuracion en una estructura.
 * Si alguno de los datos de config no se encontraron, retorna NULL;
 */
tConsola *getConfigConsola(char *ruta_config){

	// Alojamos espacio en memoria para la esctructura que almacena los datos de config
	tConsola *consola      = malloc(sizeof *consola);
	consola->ip_kernel     = malloc(MAX_IP_LEN * sizeof consola->ip_kernel);
	consola->puerto_kernel = malloc(MAX_PORT_LEN * sizeof consola->puerto_kernel);

	t_config *consola_conf = config_create(ruta_config);

	if (!config_has_property(consola_conf, "IP_KERNEL") || !config_has_property(consola_conf, "PUERTO_KERNEL")){
		printf("No se detecto alguno de los parametros de configuracion!");
		return NULL;
	}

	strcpy(consola->ip_kernel, config_get_string_value(consola_conf, "IP_KERNEL"));
	strcpy(consola->puerto_kernel, config_get_string_value(consola_conf, "PUERTO_KERNEL"));

	config_destroy(consola_conf);
	return consola;
}

void liberarConfiguracionConsola(tConsola *cons){
	free(cons->ip_kernel);
	free(cons->puerto_kernel);
	free(cons);
}
