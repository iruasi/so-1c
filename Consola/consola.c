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

#include "../Compartidas/funcionesCompartidas.c"
#include "../Compartidas/funcionesPaquetes.h"
#include "../Compartidas/tiposPaquetes.h"
#include "../Compartidas/tiposErrores.h"
#include "consolaConfigurators.h"

#define MAXMSJ 100

/* Con este macro verificamos igualdad de strings;
 * es mas expresivo que strcmp porque devuelve true|false mas humanamente
 */
#define STR_EQ(BUF, CC) (!strcmp((BUF),(CC)))

int Iniciar_Programa(char*);
int Finalizar_Programa(int process_id);
void Desconectar_Consola();
void Limpiar_Mensajes();


void Limpiar_Mensajes(void){
	int i;
	for (i = 0; i < 100; i++)
		puts();
}

int siguientePID(void){return 1;}

// todo: depende de como tengamos almacenados los PIDs (con attributos, o algunoa otra info)
// segun eso, depende la cantidad de parametros de esta funcion etc...
void Desconectar_Consola(int sock_ker, pthread_attr_t attr, tConsola cons_data){

	int pid;

	while(pid = siguientePID()){
		Finalizar_Programa(pid, sock_ker, attr);
	}

	close(sock_ker);
	liberarConfiguracionConsola(cons_data);
}



int Finalizar_Programa(int pid, int sock_ker, pthread_attr_t attr){

	tPackHeader h, j;
	h.tipo_de_mensaje = KILL_PID;
	h.tipo_de_proceso = CON;
	tPackPID ppid;
	ppid.head = h;
	ppid.pid  = pid;

	int stat;
	stat = send(sock_ker, ppid, sizeof ppid, 0);



	stat = recv(sock_ker, &j, sizeof j, 0);
	if (j.tipo_de_mensaje != KER_KILLED){
		puts("No se pudo matar");
		return FALLO_MATAR;
	}
	// ahora matamos

	pthread_attr_destroy(attr);
	return 0;
}

typedef struct {
	int sock;
	char *path;
} pathYSocket;

int Iniciar_Programa(pathYSocket *args){

	handshakeCon(args->sock, CON);
	pthread_attr_t attr;
	pthread_t hilo_prog;

	pthread_attr_init(&attr);
	pthread_attr_setdetatchstate(&attr, PTHREAD_CREATE_DETATCHED); // todo: ver si existe

	if( pthread_create(&hilo_prog, attr, (void*) programa_handler, (void*) args) < 0){
		perror("No pudo crear hilo. error");
		return FALLO_GRAL;
	}

	return 0;
}

void *programa_handler(void *argsX){

	pathYSocket *args = (pathYSocket *) argsX;
	int stat;
	tPackHeader head_tmp;

	puts("Creando codigo fuente...");
	tPackSrcCode *src_code = readFileIntoPack(CON, args->path);

	puts("Serializando codigo fuente...");
	void * paquete_serializado = serializarSrcCode(src_code);

	puts("Enviando codigo fuente...");
	int packSize = sizeof src_code->head + sizeof src_code->sourceLen + src_code->sourceLen;
	if ((stat = send(args->sock, paquete_serializado, packSize, 0)) < 0){
		perror("No se pudo enviar codigo fuente a Kernel. error");
		return FALLO_SEND;
	}

	printf("Se envio el paquete de codigo fuente...");
	// enviamos el codigo fuente, lo liberamos ahora antes de olvidarnos..
	free(src_code->sourceCode);
	free(src_code);
	free(paquete_serializado);

	tPackPID ppid;
	ppid.head = head_tmp;

	while((stat = recv(args->sock, &ppid.head, HEAD_SIZE, 0)) != -1){

		if (head_tmp.tipo_de_mensaje == RECV_PID){
			puts("recibimos PID");
			stat = recv(args->sock, &ppid.pid, sizeof ppid.pid, 0);

			agregarPrograma();

		}



		puts("Recibimos info para imprimir");

	}
}


//void readPackage(t_PackageEnvio*, tConsola*, char*);

void *serializarSrcCode(tPackSrcCode *src_code);
tPackSrcCode *readFileIntoPack(tProceso sender, char* ruta);

int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	char *buf = malloc(MAXMSJ);
	int stat;
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

	Iniciar_Programa("../CPU/facil.ansisop");

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

/* Dado un archivo, lo lee e inserta en un paquete de codigo fuente
 */
tPackSrcCode *readFileIntoPack(tProceso sender, char* ruta){
// todo: en la estructura tPackSrcCode podriamos aprovechar mejor el espacio. Sin embargo, funciona bien

	FILE *file = fopen(ruta, "rb");
	tPackSrcCode *src_code = malloc(sizeof *src_code);

	src_code->head.tipo_de_proceso = sender;
	src_code->head.tipo_de_mensaje = SRC_CODE;

	unsigned long fileSize = fsize(file) + 1; // + 1 para el '\0'
	printf("fsize es %lu\n", fileSize);

	src_code->sourceLen = fileSize;
	src_code->sourceCode = malloc(src_code->sourceLen);

	fread(src_code->sourceCode, src_code->sourceLen, 1, file);
	fclose(file);

	// ponemos un '\0' al final porque es probablemente mandatorio para que se lea, send'ee y recv'ee bien despues
	src_code->sourceCode[src_code->sourceLen - 1] = '\0';

	return src_code;
}

void *serializarSrcCode(tPackSrcCode *src_code){

	int offset = 0;

	void *serial_src_code = malloc(sizeof src_code->head + sizeof src_code->sourceLen + src_code->sourceLen);
	if (serial_src_code == NULL){
		perror("No se pudo mallocar el serial_src_code. error");
		return NULL;
	}

	memcpy(serial_src_code, &src_code->head, sizeof src_code->head);
	offset += sizeof src_code->head;

	memcpy(serial_src_code + offset, &src_code->sourceLen, sizeof src_code->sourceLen);
	offset += sizeof src_code->sourceLen;

	puts("esto vamos a meter en serializado");
	puts(src_code->sourceCode);

	memcpy(serial_src_code + offset, src_code->sourceCode, src_code->sourceLen);

	return serial_src_code;
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

