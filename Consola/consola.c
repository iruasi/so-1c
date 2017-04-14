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
#define FALLO_SOCKET -23
#define FALLO_CONEXION -25

tConsola *getConfigConsola(char *ruta_archivo_configuracion);
void mostrarConfiguracionConsola(tConsola *datos_consola);
void liberarConfiguracionConsola(tConsola *datos_consola);


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

	char buf[MAXMSJ] = "go";
	int i;
	int stat = 1;
	int sock_kern;

	tConsola *cons_data = getConfigConsola(argv[1]);
	if (cons_data == NULL)
		return FALLO_CONFIGURACION;
	mostrarConfiguracionConsola(cons_data);

	sock_kern = establecerConexion(cons_data->ip_kernel, cons_data->puerto_kernel);
	if (sock_kern < 0)
		return sock_kern;

	// todo: ESTE CICLO NO ANDA BIEN, NO SE PORQUE
	printf(" buf: %s\n stat: %d\n", buf, stat);
	while((strcmp(buf, "\n\0")) && stat != -1){

		printf("Ingrese su mensaje:\n");
		fgets(buf, MAXMSJ -2, stdin);
		buf[MAXMSJ -1] = '\0';

		printf("enviando...%s\n", buf);
		stat = send(sock_kern, buf, MAXMSJ, 0);

		for(i = 0; i < MAXMSJ; i++)
			buf[i] = '\0';
	}

	printf(" buf: %s\n stat: %d\n", buf, stat);
	printf("Cerrando comunicacion y limpiando proceso...\n");

	close(sock_kern);
	liberarConfiguracionConsola(cons_data);
	return 0;
}


void setupHints(struct addrinfo *hint, int fam, int sockType, int flags){

	memset(hint, 0, sizeof *hint);
	hint->ai_family = fam;
	hint->ai_socktype = sockType;
	hint->ai_flags = flags;
}

/* Dados un ip y puerto de destino, se crea, conecta y retorna socket apto para comunicacion
 * La deberia utilizar unicamente Iniciar_Programa, por cada nuevo hilo para un script que se crea
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

void mostrarConfiguracionConsola(tConsola *cons_data){

	printf("%s\n", cons_data->ip_kernel);
	printf("%s\n", cons_data->puerto_kernel);
}

void liberarConfiguracionConsola(tConsola *cons){

	free(cons->ip_kernel);
	free(cons->puerto_kernel);
	free(cons);
}
