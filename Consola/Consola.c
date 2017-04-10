/*
 * Consola.c
 *
 *  Created on: 10/4/2017
 *      Author: utnso
 */

/*
 Estos includes los saque de la guia Beej... puede que ni siquiera los precisemos,
 pero los dejo aca para futuro, si algo mas complejo no anda tal vez sirvan...

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
*/

#include <stdio.h>
#include <string.h>
#include <netdb.h>

#include <commons/config.h>

#define MAX_IP_LEN 17
#define MAX_PORT_LEN 6
#define MAXLEN 100

#define FALLO_GRAL -21
#define FALLO_CONFIGURACION -22


int setupConfig(t_config *configuracion);
void setupHints(struct addrinfo *hints, int address_family, int socket_type, int flags);
int establecerConexion();

void Iniciar_Programa();
void Finalizar_Programa(int process_id);
void Desconectar_Consola();
void Limpiar_Mensajes();

// Son variables globales para no andar pasandolas como parametro
// a cada thread que se vaya a crear...
char *ip_kernel[MAX_IP_LEN];
char *port_kernel[MAX_PORT_LEN];

int main(void){

	t_config *conf = config_create("config");
	if (setupConfig(conf) < 0)
		return FALLO_CONFIGURACION;

	printf ("ip es:%s\npuerto es:%s\n", ip_kernel, port_kernel);



	return 0;
}


int setupConfig(t_config* cfg){

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

int establecerConexion(){

	char * msj = "Hola! Soy un script en consola!";
//	int bytes_sent; // Se deberia usar despues para chequear que send() procedio bien

	int stat;
	int sock_kern; // file descriptor para el socket del kernel
	struct addrinfo hints, *serverInfo;

	setupHints(&hints, AF_UNSPEC, SOCK_STREAM, 0);

	if ((stat = getaddrinfo(ip_kernel, port_kernel, &hints, &serverInfo)) != 0){
		fprintf("getaddrinfo: %s\n", gai_strerror(stat));
		return FALLO_GRAL;
	}

	sock_kern = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol);

// Por ahora no utilizamos connect() porque precisamos que el kernel funcione
	connect(sock_kern, serverInfo->ai_addr, serverInfo->ai_addrlen);
	freeaddrinfo(serverInfo);

	// Por ahora no utilizamos send() porque precisamos el kernel funcione
	send(sock_kern, msj, strlen(msj), 0); // send retorna un int, pero no nos interesa para error checking aun

	return sock_kern;
}


