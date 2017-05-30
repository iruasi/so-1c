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
#include <errno.h>

#include "../Compartidas/funcionesCompartidas.h"
#include "../Compartidas/funcionesPaquetes.h"
#include "../Compartidas/tiposPaquetes.h"
#include "../Compartidas/tiposErrores.h"
#include "consolaConfigurators.h"
#include "apiConsola.h"

#define MAXMSJ 100

/* Con este macro verificamos igualdad de strings;
 * es mas expresivo que strcmp porque devuelve true|false mas humanamente
 */
#define STR_EQ(BUF, CC) (!strcmp((BUF),(CC)))


int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	char *buf = malloc(MAXMSJ);
	int stat = 0;
	int sock_kern;

	tConsola *cons_data = getConfigConsola(argv[1]);
	mostrarConfiguracionConsola(cons_data);

	printf("Conectando con kernel...\n");
	sock_kern = establecerConexion(cons_data->ip_kernel, cons_data->puerto_kernel);
	if (sock_kern < 0){
		errno = FALLO_CONEXION;
		perror("No se pudo establecer conexion con Kernel. error");
		return sock_kern;
	}

	tPathYSock *args = malloc(sizeof *args);
	args->sock = sock_kern;
	args->path = "../CPU/facil.ansisop";
	Iniciar_Programa(args);

	while(!(STR_EQ(buf, "terminar\n")) && (stat != -1)){

		printf("Ingrese su mensaje: (esto no realiza ninguna accion en realidad)\n");
		fgets(buf, MAXMSJ, stdin);
		buf[MAXMSJ -1] = '\0';

//		stat = send(sock_kern, buf, MAXMSJ, 0);

		clearBuffer(buf, MAXMSJ);
	}


	printf("Cerrando comunicacion y limpiando proceso...\n");


	return 0;
}


// De momento no necesitamos esta para enviar codigo fuente..
// puede que sea necesaria para enviar otras cosas. La dejo comentada
//char* serializarOperandos(t_PackageEnvio *package){
//
//	char *serializedPackage = malloc(package->total_size);
//	int offset = 0;
//	int size_to_send;
//
//
//	size_to_send =  sizeof(package->tipo_de_proceso);
//	memcpy(serializedPackage + offset, &(package->tipo_de_proceso), size_to_send);
//	offset += size_to_send;
//
//
//	size_to_send =  sizeof(package->tipo_de_mensaje);
//	memcpy(serializedPackage + offset, &(package->tipo_de_mensaje), size_to_send);
//	offset += size_to_send;
//
//	size_to_send =  sizeof(package->message_size);
//	memcpy(serializedPackage + offset, &(package->message_size), size_to_send);
//	offset += size_to_send;
//
//	size_to_send =  package->message_size;
//	memcpy(serializedPackage + offset, package->message, size_to_send);
//
//	return serializedPackage;
//}
int recieve_and_deserialize(t_PackageRecepcion *package, int socketCliente){

	int status;
	int buffer_size;
	char *buffer = malloc(buffer_size = sizeof(uint32_t));
	clearBuffer(buffer,buffer_size);

	uint32_t tipo_de_proceso;
	status = recv(socketCliente, buffer, sizeof(package->tipo_de_proceso), 0);
	memcpy(&(tipo_de_proceso), buffer, buffer_size);
	if (!status) return 0;

	uint32_t tipo_de_mensaje;
	status = recv(socketCliente, buffer, sizeof(package->tipo_de_mensaje), 0);
	memcpy(&(tipo_de_mensaje), buffer, buffer_size);
	if (!status) return 0;


	uint32_t message_size;
	status = recv(socketCliente, buffer, sizeof(package->message_size), 0);
	memcpy(&(message_size), buffer, buffer_size);
	if (!status) return 0;

	status = recv(socketCliente, package->message, message_size, 0);
	if (!status) return 0;

	printf("%d %d %s",tipo_de_proceso,tipo_de_mensaje,package->message);

	free(buffer);

	return status;
}

//
//void readPackage(t_PackageEnvio* package, tConsola* cons_data, char* ruta){
//	package->tipo_de_proceso = cons_data->tipo_de_proceso;
//	package->tipo_de_mensaje = 2;
//	FILE *f = fopen(ruta, "rb");
//	fseek(f, 0, SEEK_END);
//	long fsize = ftell(f);
//	fseek(f, 0, SEEK_SET);  //same as rewind(f);
//
//	char *string = malloc(fsize + 1);
//	package->message = malloc(fsize);
//	fread(string, fsize, 1, f);
//	fclose(f);
//	string[fsize] = 0;
//
//	strcpy(package->message,string);
//
//	package->message_size = strlen(package->message)+1;
//	package->total_size = sizeof(package->tipo_de_proceso)+sizeof(package->tipo_de_mensaje)+sizeof(package->message_size)+package->message_size+sizeof(package->total_size);
//
//}

