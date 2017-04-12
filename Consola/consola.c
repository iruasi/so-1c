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

#define MAX_IP_LEN 17
#define MAX_PORT_LEN 6
#define MAXMSJ 100

#define FALLO_GRAL -21
#define FALLO_CONFIGURACION -22


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

	char *ip_kernel = malloc(MAX_IP_LEN * sizeof *ip_kernel);
	char *port_kernel = malloc(MAX_PORT_LEN * sizeof *port_kernel);

	t_config *conf = config_create(argv[1]);
	if (setupConfig(conf, ip_kernel, port_kernel) < 0)
		return FALLO_CONFIGURACION;

	printf ("ip es: %s\npuerto es: %s\n", ip_kernel, port_kernel);

	int sock = establecerConexion(ip_kernel, port_kernel);
	printf("El socket usado fue %d", sock);

	free(ip_kernel);
	free(port_kernel);

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

