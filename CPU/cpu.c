#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>

#include <commons/config.h>

#define MAX_IP_LEN 16 // Este largo solo es util para IPv4
#define MAX_PORT_LEN 6

#define MAXMSJ 100 // largo maximo de mensajes a enviar. Solo utilizado para 1er checkpoint

#define FALLO_GRAL -21
#define FALLO_CONFIGURACION -22


int setupConfig(t_config *configuracion, char *ip_kernel, char *ip_memoria, char *puerto_kernel, char *puerto_memoria);
int revisarYObtenerConf(t_config *configuracion, char *entrada_del_valor, char* valor_de_configuracion);

void setupHints(struct addrinfo *hints, int address_family, int socket_type, int flags);
int establecerConexion(char *ip_destino, char *puerto_destino);

int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	int stat;
	int sock_kern, sock_mem;

	char *ip_memoria = malloc(MAX_IP_LEN * sizeof *ip_memoria);
	char *ip_kernel  = malloc(MAX_IP_LEN * sizeof *ip_kernel);

	char *port_memoria = malloc (MAX_PORT_LEN * sizeof *port_memoria);
	char *port_kernel  = malloc (MAX_PORT_LEN * sizeof *port_kernel);

	t_config *conf = config_create(argv[1]);

	if ((stat = setupConfig(conf, ip_kernel, ip_memoria, port_kernel, port_memoria)) < 0)
		return stat;

	printf ("ip de kernel es:%s\npuerto kernel es:%s\n", ip_kernel, port_kernel);
	printf ("ip de memoria es:%s\npuerto memoria es:%s\n", ip_memoria, port_memoria);

	printf("Conectando con kernel...\n");
	sock_kern = establecerConexion(ip_kernel, port_kernel);
	printf("socket de kernel es: %d\n",sock_kern);

//	printf("Conectando con memoria...\n");
//	sock_mem = establecerConexion(ip_memoria, port_memoria);
//	printf("socket de memoria es: %d\n",sock_mem);

	return 0;
}

/* Con esta funcion se cargan los valores pertinentes del config_cpu;
 * retorna FALLO_CONFIGURACION si algo sale mal. Retorna 0 en caso contrario.
 */
int setupConfig(t_config* cfg, char *ip_kern, char *ip_mem, char *port_kern, char *port_mem){

	int stat;

	if ((stat = revisarYObtenerConf(cfg, ip_kern, "IP_KERNEL")) != 0)
		return stat;

	if ((stat = revisarYObtenerConf(cfg, port_kern, "PUERTO_KERNEL")) != 0)
		return stat;

	if ((stat = revisarYObtenerConf(cfg, ip_mem, "IP_MEMORIA")) != 0)
		return stat;

	if ((stat = revisarYObtenerConf(cfg, port_mem, "PUERTO_MEMORIA")) != 0)
		return stat;

	return 0;
}

/* Si `valor' existe en `cfg', lo carga como String en `entrada' y retorna 0;
 * Caso contrario retorna FALLO_CONFIGURACION.
 */
int revisarYObtenerConf(t_config *cfg, char *entrada, char* valor){

	if (!config_has_property(cfg, valor)){
				printf("No se detecto el valor %s!", valor);
				return FALLO_CONFIGURACION;
	}
	strcpy(entrada, config_get_string_value(cfg, valor));

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

	char * msj = "Hola soy cpu!";

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

