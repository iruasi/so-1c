#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <commons/collections/list.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <commons/log.h>

#include "apiConsola.h"
#include "auxiliaresConsola.h"
#include "consolaConfigurators.h"

#include <tiposRecursos/tiposErrores.h>
#include <tiposRecursos/tiposPaquetes.h>
#include <funcionesPaquetes/funcionesPaquetes.h>
#include <funcionesCompartidas/funcionesCompartidas.h>

#define SIGDISCONNECT 80


int tamanioPrograma;

extern t_list *listaAtributos;
extern t_log *logTrace;
extern tConsola *cons_data;

pthread_mutex_t mux_listaAtributos,mux_listaFinalizados;


void accionesFinalizacion(int pid);


void inicializarSemaforos(void){
	pthread_mutex_init(&mux_listaAtributos,NULL);
	pthread_mutex_init(&mux_listaFinalizados,NULL);


}

void eliminarSemaforos(void){
	//sem_destroy(&semLista);
}


int Iniciar_Programa(tAtributosProg *atributos){
	FILE *file = fopen(atributos->path, "rb");
	if(file != NULL){
		pthread_attr_t attr;
		pthread_t hilo_prog;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

		if(pthread_create(&hilo_prog, &attr, (void *) programa_handler, (void*) atributos) < 0){
			log_error(logTrace,"No pudo crear hilo.");
			return FALLO_GRAL;
		}
		log_trace(logTrace,"hilo de programa creado");

		return 0;
	}
	else{
		puts("ruta invalida intente de nuvo");
		return 0;
	}
}

int Finalizar_Programa(int pid){

	int stat;
	int pack_size;
	char *pid_serial;
	tPackPID pack_pid;
	tAtributosProg *attr;

	pthread_mutex_lock(&mux_listaAtributos);
	if ((attr = getAttrProgDeLista(pid)) == NULL){
		pthread_mutex_unlock(&mux_listaAtributos);
		return FALLO_GRAL;
	}
	pthread_mutex_unlock(&mux_listaAtributos);

	pack_pid.head.tipo_de_proceso = CON; pack_pid.head.tipo_de_mensaje = KILL_PID;
	pack_pid.val = pid;

	pack_size = 0;

	if ((pid_serial = serializeVal(&pack_pid, &pack_size)) == NULL){
		log_error(logTrace,"fallo en la serializacion");
		return FALLO_SERIALIZAC;
	}

	if ((stat = send(attr->sock, pid_serial, pack_size, 0)) == -1){
		perror("Fallo envio de PID a Consola. error");
		return FALLO_SEND;
	}
	log_trace(logTrace,"se enviaron %d bytes al kernel",stat);

	//printf("Se enviaron %d bytes al kernel\n", stat);

	free(pid_serial);


	//habria q  ver si importa que el kernel conteste o matarlo de una y fue..
	//accionesFinalizacion(pid);

//	if((stat = recv(sock_kern, &recv_head, sizeof recv_head, 0)) == -1){
//		log_error(logger,"Fallo de recepcion respuesta Kernel");
//		return FALLO_RECV;
//
//	} else if (stat == 0){
//		log_trace(logTrace,"Kernel cerro la conexion, antes de dar una respuesta...");
//		return FALLO_GRAL;
//	}
//
//
//	if (recv_head.tipo_de_mensaje == KER_KILLED){
//		log_info(logger,"Tenemos la confirmacion de kernel, termino con el proceso");
//		accionesFinalizacion(pid);
//		return FALLO_MATAR;
//	}
//
//	if (recv_head.tipo_de_mensaje != KER_KILLED){
//		log_info(logger,"No se pudo matar (o no se recibio la confirmacion)");
//		return FALLO_MATAR;
//	}


	return 0;
}
void accionesFinalizacion(int pid){
	int i;
	char buffInicio[100];
    char buffFin[100];
    pthread_mutex_lock(&mux_listaAtributos);
    for(i = 0; i < list_size(listaAtributos); i++){
		tAtributosProg *aux = list_get(listaAtributos, i);
		if(aux->pidProg==pid){

			time(&aux->horaFin);
			double diferencia = difftime(aux->horaFin, aux->horaInicio);

			strftime (buffInicio, 100, "%Y-%m-%d %H:%M:%S.000", localtime (&aux->horaInicio));
			strftime (buffFin, 100, "%Y-%m-%d %H:%M:%S.000", localtime (&aux->horaFin));

			printf("\n\n\n###FIN DE LA EJECUCION DEL PROCESO %d \n###INFO DE EJECUCION: \n",pid);

			printf ("###HORA DE INICIO: %s\n", buffInicio);
			printf ("###HORA DE FIN: %s\n", buffFin);
			printf("###SEGUNDOS DE EJECUCION: %.f segundos \n",diferencia);
			printf("###CANTIDAD DE IMPRESIONES POR PANTALLA: %d\n",aux->cantImpresiones);//todo:impresiones x pantala

			log_info(logTrace,"FIN DE EJECUCION PROCESO %d",pid);
			log_info(logTrace,"HORA DE INICIO: %s",buffInicio);
			log_info(logTrace,"HORA DE FIN: %s",buffFin);
			log_info(logTrace,"SEGUNDOS DE EJECUCION: %.f",diferencia);
			log_info(logTrace,"CANTIDAD DE IMPRESIONES X PANTALLA: %d",aux->cantImpresiones);

			//Lo sacamos de la lista de programas en ejecucion

			//list_remove(listaAtributos,i);

			//close(sock_kern); //todo:revisar esto

			//pthread_cancel(aux->hiloProg);
		}
	}
	pthread_mutex_unlock(&mux_listaAtributos);

}

void Desconectar_Consola(){
	log_trace(logTrace,"eligio desconectar la consola");

	int i;
	pthread_mutex_lock(&mux_listaAtributos);
	for(i = 0; i < list_size(listaAtributos); i++){
		tAtributosProg *aux = list_get(listaAtributos, i);
		log_trace(logTrace,"pid %d dead \n",aux->pidProg);
		close(aux->sock);
		pthread_cancel(aux->hiloProg);
		//list_remove(listaAtributos,i);
	}
	list_clean(listaAtributos);

	pthread_mutex_unlock(&mux_listaAtributos);

	log_trace(logTrace,"todos los programas muertos");

}


void Limpiar_Mensajes(){
	log_trace(logTrace,"limpiar mensajes");
	system("clear");
}

void *programa_handler(void *atributos){
	int finalizar = 0;
	char *buffer;
	char buffInicio[100];
	int pack_size;
	int stat;

	tAtributosProg *args = (tAtributosProg *) atributos;

	args->sock = establecerConexion(cons_data->ip_kernel, cons_data->puerto_kernel);
	args->hiloProg = pthread_self();
	if (args->sock < 0){
		errno = FALLO_CONEXION;
		log_error(logTrace, "no se pudo establecer conexion con kernel.");
		return NULL;
	}
	args->cantImpresiones = 0;

	time(&args->horaInicio);
	log_trace(logTrace,"momento de inicio programa [%s]",args->path);
	strftime (buffInicio, 100, "%Y-%m-%d %H:%M:%S.000", localtime (&args->horaInicio));
	printf ("Hora de inicio: %s\n", buffInicio);

	handshakeCon(args->sock, CON);
	log_trace(logTrace,"handshake realizado");
	tPackHeader head_tmp;

	log_trace(logTrace,"creando codigo fuente");
	tPackSrcCode *src_code = readFileIntoPack(CON, args->path);
	log_trace(logTrace,"codigo fuente creado");
	log_trace(logTrace,"serializando codigo fuente");
	head_tmp.tipo_de_proceso = CON; head_tmp.tipo_de_mensaje = SRC_CODE; pack_size = 0;
	buffer = serializeBytes(head_tmp, src_code->bytes, src_code->bytelen, &pack_size);

	log_trace(logTrace,"codigo fuente serializado");


	log_trace(logTrace,"conectando con kernel");

	log_trace(logTrace,"enviando codigo fuente");

	if ((stat = send(args->sock, buffer, pack_size, 0)) == -1){
		log_error(logTrace,"no se pudo enviar codigo fuente a kernel. ");
		return (void *) FALLO_SEND;
	}
	log_trace(logTrace,"se enviaron %d bytes",stat);


	// enviamos el codigo fuente, lo liberamos ahora antes de olvidarnos..
	freeAndNULL((void **) &src_code->bytes);
	freeAndNULL((void **) &src_code);
	freeAndNULL((void **) &buffer);

	tPackPID *ppid;
	log_trace(logTrace,"esperando a recibir el PID");
	while(finalizar != 1){
		puts("a");
		while((stat = recv(args->sock, &(head_tmp), HEAD_SIZE, 0)) > 0){

			if (head_tmp.tipo_de_mensaje == IMPRIMIR){
				//log_trace(logTrace,"kernel manda algo a imprimir [PID %d]",args->pidProg);

				log_trace(logTrace, "recibimos info para imprimir[PID %d]",args->pidProg);

				if ((buffer = recvGeneric(args->sock)) == NULL){
					log_error(logTrace,"error al recibir la info a imprimir[PID %d]",args->pidProg);
					return (void *) FALLO_RECV;
				}

				tPackRW * prints=deserializeRW(buffer);

				printf("[Impresion para PID:%d]: %s\n",args->pidProg, (char *) prints->info);
				args->cantImpresiones++;

			}


			if (head_tmp.tipo_de_mensaje == PID){
				log_trace(logTrace,"recibimos PID");

				if ((buffer = recvGeneric(args->sock)) == NULL){
					log_error(logTrace,"error al recibir el pid");
					return (void *) FALLO_RECV;
				}

				if ((ppid = deserializeVal(buffer)) == NULL){
					log_error(logTrace,"error al deserializar el packPID");
					return (void *) FALLO_DESERIALIZAC;
				}

				log_trace(logTrace,"asigno pid a la estructura");

				memcpy(&args->pidProg, &ppid->val, sizeof(int));
				freeAndNULL((void **)&ppid);


				log_trace(logTrace,"PID: %d ",args->pidProg);

				printf("Pid Recibido es: %d\n", args->pidProg);


				pthread_mutex_lock(&mux_listaAtributos);
				list_add(listaAtributos,args);
				pthread_mutex_unlock(&mux_listaAtributos);

			}


			if (head_tmp.tipo_de_mensaje == FIN_PROG){

				log_trace(logTrace,"Kernel nos avisa que termino de ejecutar el programa %d",args->pidProg);

				printf("Kernel nos avisa que termino de ejecutar el programa %d\n", args->pidProg);

				accionesFinalizacion(args->pidProg);

				finalizar=1;
				//close(args->sock); //todo:revisar esto


				//pthread_cancel(aux->hiloProg);

			}

			if((int) head_tmp.tipo_de_mensaje == DESCONEXION_CPU){
				printf("\n\n\n #####Kernel nos avisa q termino de ejecutar el programa %d (por desconexion de CPU)\n####",args->pidProg);
				log_trace(logTrace,"Kernel nos avisa q termino de ejecutar el programa %d (por desconexion de CPU)",args->pidProg);
				accionesFinalizacion(args->pidProg);
				finalizar=1;
				//close(args->sock);
			}


		}
	}



	if (stat == -1){
		log_error(logTrace,"fallo recepcion header en thread de programa");
		return (void *) FALLO_RECV;
	}
	log_trace(logTrace,"kernel cerro conexion con thread de programa");
	log_trace(logTrace,"hilo del programa %d finalizado",args->pidProg);
	close(args->sock);
	return NULL;
}













