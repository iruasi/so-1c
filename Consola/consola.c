#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

#include <commons/collections/list.h>
#include <commons/log.h>

#include <funcionesPaquetes/funcionesPaquetes.h>
#include <funcionesCompartidas/funcionesCompartidas.h>
#include <tiposRecursos/tiposPaquetes.h>
#include <tiposRecursos/tiposErrores.h>

#include "consolaConfigurators.h"
#include "apiConsola.h"

#define MAXMSJ 200


/*Agarra los ultimos char de un string (para separar la ruta en la instruccion Nuevo programa <ruta>)
 */


/* Con este macro verificamos igualdad de strings;
 * es mas expresivo que strcmp porque devuelve true|false mas humanamente
 */
#define STR_EQ(BUF, CC) (!strcmp((BUF),(CC)))

t_list *listaAtributos,*listaFinalizados,listaHilos;

tConsola *cons_data;

t_log *logTrace;

int main(int argc, char* argv[]){




	logTrace = log_create("/home/utnso/logCONSOLATrace.txt","CONSOLA",0,LOG_LEVEL_TRACE);

	log_trace(logTrace,"");
	log_trace(logTrace,"");
	log_trace(logTrace,"");
	log_trace(logTrace,"");
	log_trace(logTrace,"");
	log_trace(logTrace,"");
	log_trace(logTrace,"");
	log_trace(logTrace,"Inicia nueva ejecucion de CONSOLA");
	log_trace(logTrace,"");
	log_trace(logTrace,"");
	log_trace(logTrace,"");
	log_trace(logTrace,"");
	log_trace(logTrace,"");
	log_trace(logTrace,"");

	inicializarSemaforos();
	if(argc!=2){
		log_error(logTrace,"Error en la cantidad de parametros");
		return EXIT_FAILURE;
	}

	//Creo lista de programas para ir agregando a medida q vayan iniciandose.
	listaAtributos = list_create();
	listaFinalizados = list_create();
	cons_data = getConfigConsola(argv[1]);
	mostrarConfiguracionConsola(cons_data);



	printf("\n\n\n###Ingrese accion a realizar:\n");
	printf ("###Para iniciar un programa: 'nuevo programa <ruta>'\n");
	printf ("###Para finlizar un programa: 'finalizar <PID>'\n");
	printf ("###Para desconectar consola: 'desconectar'\n");
	printf ("###Para limpiar mensajes: 'limpiar'\n");
	int finalizar = 0;

	while(finalizar !=1){
		printf("###SELECCIONE OPCION###: \n");
		char *opcion=malloc(MAXMSJ);
		fgets(opcion,MAXMSJ,stdin);
		int status;
		opcion[strlen(opcion) - 1] = '\0';
		if (strncmp(opcion,"nuevo programa",14)==0)
		{
			log_trace(logTrace,"eligio opcion nuevo programa");
			char *ruta = opcion+15;

			//TODO:Chequear error de ruta..
			tAtributosProg *atributos = malloc(sizeof *atributos);
			atributos->sock = -1;
			atributos->path = ruta;

			//printf("Ruta del programa: %s\n",atributos->path);
			log_trace(logTrace,"ruta del programa: %s",ruta);

			printf("Ruta del programa a iniciar: %s\n",ruta);

			status = Iniciar_Programa(atributos);
			if(status<0){
				log_error(logTrace,"error al iniciar programa");
				return FALLO_GRAL;
			}

		}
		if(strncmp(opcion,"finalizar",9)==0)
		{
			log_trace(logTrace,"finalizar programa");
			char *pidString = opcion+10;
			int pidElegido = atoi(pidString);
			log_trace(logTrace,"eligio opcion finalizar el pid: %d",pidElegido);
			status = Finalizar_Programa(pidElegido);
		}
		if(strncmp(opcion,"desconectar",11)==0){
			log_trace(logTrace,"eligio opcion desconectar consola");
			Desconectar_Consola();

			//finalizar = 1;
			//close(sock_kern);//todo: revisar esto
			//liberarConfiguracionConsola(cons_data);
		}
		if(strncmp(opcion,"limpiar",7)==0){
			log_trace(logTrace,"eligio opcion limpiar pantalla");
			Limpiar_Mensajes();
		}

	}


	log_trace(logTrace,"cerrando comunicacion y limpiando proceso");


	log_destroy(logTrace);

	return 0;
}






