#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <commons/collections/list.h>

#include <funcionesPaquetes/funcionesPaquetes.h>
#include <funcionesCompartidas/funcionesCompartidas.h>
#include <tiposRecursos/tiposPaquetes.h>
#include <tiposRecursos/tiposErrores.h>

#include "consolaConfigurators.h"
#include "apiConsola.h"

#define MAXMSJ 100

/*Agarra los ultimos char de un string (para separar la ruta en la instruccion Nuevo programa <ruta>)
 */
void strncopylast(char *,char *,int );


/* Con este macro verificamos igualdad de strings;
 * es mas expresivo que strcmp porque devuelve true|false mas humanamente
 */
#define STR_EQ(BUF, CC) (!strcmp((BUF),(CC)))

//TODO: Esto global?!??
t_list *listaProgramas;

int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	int sock_kern;
	//Creo lista de programas para ir agregando a medida q vayan iniciandose.

	listaProgramas = list_create();
	// {hilo, pid}
	tConsola *cons_data = getConfigConsola(argv[1]);
	mostrarConfiguracionConsola(cons_data);

	printf("Conectando con kernel...\n");
	sock_kern = establecerConexion(cons_data->ip_kernel, cons_data->puerto_kernel);
	if (sock_kern < 0){
		errno = FALLO_CONEXION;
		perror("No se pudo establecer conexion con Kernel. error");
		return sock_kern;
	}



	//TODO:No habria q hacer handsakhe aca en lugar de hacerlo cuando iniciamos un programa?
	puts("Nos conectamos a kernel");
	printf("\n \n \nIngrese accion a realizar:\n");
	int finalizar = 0;
	while(finalizar !=1){

		printf("Para iniciar un programa: 'nuevo programa <ruta>'\n");
		printf ("Para finlizar un programa: 'finalizar <PID>'\n");
		printf ("Para desconectar consola: 'desconectar'\n");
		printf ("Para limpiar mensajes: 'limpiar'\n");



		char *opcion=malloc(MAXMSJ);
		fgets(opcion,MAXMSJ,stdin);
		opcion[strlen(opcion) - 1] = '\0';
		if (strncmp(opcion,"nuevo programa",14)==0)
		{
			printf("Iniciar un nuevo programa\n");
			char *ruta = opcion+15;

			tPathYSock *args = malloc(sizeof *args);
			args->sock = sock_kern;
			args->path = ruta;

			printf("Ruta del programa: %s\n",args->path);

			int status = Iniciar_Programa(args);
			if(status<0){
				puts("Error al iniciar programa");
				//TODO: Crear FALLO_INICIARPROGRAMA
				return -1000;
			}

		}
		if(strncmp(opcion,"finalizar",9)==0)
		{
			char* pidString=malloc(MAXMSJ);
			int longitudPid = strlen(opcion) - 10;
			strncopylast(opcion,pidString,longitudPid);
			int pidElegido = atoi(pidString);
			printf("Eligio finalizar el programa %d\n",pidElegido);
			//TODO: Buscar de la lista de hilos cual sería el q corresponde al pid q queremos matar
			//int status = Finalizar_Programa(pidElegido,sock_kern,HILOAMATAR);
		}
		if(strncmp(opcion,"desconectar",11)==0){
			//int status = Desconectar_Consola()
			printf("Eligio desconectar esta consola !\n");
			finalizar = 1;
		}
		if(strncmp(opcion,"limpiar",7)==0){
			Limpiar_Mensajes();
			printf("Eligio limpiar esta consola \n");
		}

	}




	/*tPathYSock *args = malloc(sizeof *args);
	args->sock = sock_kern;
	args->path = "/home/utnso/git/tp-2017-1c-Flanders-chip-y-asociados/CPU/facil.ansisop";
	int status = Iniciar_Programa(args);

	printf("El satus es: %d\n",status);

	while(!(STR_EQ(buf, "terminar\n")) && (stat != -1)){

		printf("Ingrese su mensaje: (esto no realiza ninguna accion en realidad)\n");
		fgets(buf, MAXMSJ, stdin);
		buf[MAXMSJ -1] = '\0';

//		stat = send(sock_kern, buf, MAXMSJ, 0);

		clearBuffer(buf, MAXMSJ);
	}
*/

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


//Agarra los ultimos char de un string (para separar la ruta en la instruccion Nuevo programa <ruta>)
void strncopylast(char *str1,char *str2,int n)
{   int i;
    int l=strlen(str1);
    if(n>l)
    {
        printf("\nCan't extract more characters from a smaller string.\n");
        exit(1);
    }
    for(i=0;i<l-n;i++)
         str1++;
    for(i=l-n;i<l;i++)
    {
        *str2=*str1;
         str1++;
         str2++;
    }
    *str2='\0';
}


int agregarAListaDeProgramas(int pid){

	int tamanioAntes = list_size(listaProgramas);
	list_add(listaProgramas, &pid);
	int tamanioDespues = list_size(listaProgramas);
	if(tamanioDespues != (tamanioAntes + 1)){
		puts("error al agregar el pid a la lista");
		//TODO: LIST_CREATE_PROBLEM
		return -99;
	}
		puts("Tamanio actual de la lista:");
		printf("%d\n",tamanioDespues);
		puts("Pid agregado:");
		printf("%d\n",pid);


	//TODO: agregar PROGRAM_ADDED
	return 1;

}

