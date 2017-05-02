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
#include <unistd.h>
/* #include <sys/stat.h> funciones para obtener el tamaño de un archivo , habilitar luego de la correcta localización del metodo
#include <errno.h> */

#include "../Compartidas/funcionesCompartidas.c"
#include "../Compartidas/tiposErrores.h"
#include "consolaConfigurators.h"

#define MAXMSJ 100

/* Con este macro verificamos igualdad de strings;
 * es mas declarativo que strcmp porque devuelve true|false mas humanamente
 */
#define STR_EQ(BUF, CC) (!strcmp((BUF),(CC)))

void Iniciar_Programa();
void Finalizar_Programa(int process_id);
void Desconectar_Consola();
void Limpiar_Mensajes();
void enviarArchivo(FILE*, uint32_t, uint32_t);

int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	char *buf = malloc(MAXMSJ);
	int stat = 1;
	int sock_kern;

	tConsola *cons_data = getConfigConsola(argv[1]);
	mostrarConfiguracionConsola(cons_data);

	printf("Conectando con kernel...\n");
	sock_kern = establecerConexion(cons_data->ip_kernel, cons_data->puerto_kernel);
	if (sock_kern < 0)
		return sock_kern;

	FILE* algo = fopen("config_consola", "r");
	printf("%s", (char*) algo);
	enviarArchivo(algo, cons_data->tipo_de_proceso, sock_kern);

	while(!(STR_EQ(buf, "terminar")) && (stat != -1)){

		printf("Ingrese su mensaje:\n");
		fgets(buf, MAXMSJ, stdin);
		buf[MAXMSJ -1] = '\0';

		stat = send(sock_kern, buf, MAXMSJ, 0);

		clearBuffer(buf, MAXMSJ);
	}


	printf("Cerrando comunicacion y limpiando proceso...\n");

	close(sock_kern);
	liberarConfiguracionConsola(cons_data);
	return 0;
}

/*
 * devuelve el tamaño del archivo en long int
 * fuente https://goo.gl/cGtLra
 *
off_t tamArchivo(char *filename) {
    struct stat st;

    if (stat(nomArchivo, &st) == 0)
        return st.st_size;

    fprintf(stderr, "No se puede determinar el tamaño de: %s: %s\n",
            nomArchivo, strerror(errno));

    return -1;
}
*/


char* serializarOperandos(t_Package *package){

	char *serializedPackage = malloc(package->total_size);
	int offset = 0;
	int size_to_send;


	size_to_send =  sizeof(package->tipo_de_proceso);
	memcpy(serializedPackage + offset, &(package->tipo_de_proceso), size_to_send);
	offset += size_to_send;


	size_to_send =  sizeof(package->tipo_de_mensaje);
	memcpy(serializedPackage + offset, &(package->tipo_de_mensaje), size_to_send);
	offset += size_to_send;

	size_to_send =  sizeof(package->archivo_size);
	memcpy(serializedPackage + offset, &(package->archivo_size), size_to_send);
	offset += size_to_send;

	size_to_send =  package->archivo_size;
	memcpy(serializedPackage + offset, package->archivo_a_enviar, size_to_send);

	return serializedPackage;
}

void enviarArchivo(FILE* algo, uint32_t tipo_de_proceso, uint32_t sock_kern){
	t_Package package;
			package.tipo_de_proceso = tipo_de_proceso;
			package.tipo_de_mensaje = 2;
			//package.message = buf;
			//package.message_long = strlen(package.message)+1;
			package.archivo_size = sizeof(algo);
			package.archivo_a_enviar = algo;
			package.total_size = sizeof(package.tipo_de_proceso)+sizeof(package.tipo_de_mensaje)+sizeof(package.archivo_size)+package.archivo_size+sizeof(package.total_size);

			char *serializedPackage;
			serializedPackage = serializarOperandos(&package);

			send(sock_kern, serializedPackage, package.total_size, 0);
}
