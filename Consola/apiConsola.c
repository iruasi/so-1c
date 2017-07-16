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


int siguientePID(void){return 1;}
int tamanioPrograma;
int sock_kern;
extern t_list *listaAtributos,*listaFinalizados;
extern t_log *logger,*logTrace;
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

void differenceBetweenTimePeriod(tHora start, tHora stop, tHora *diff)
{
	if(start.segundos > stop.segundos){
		--start.minutos;
		start.segundos += 60;
	}

	diff->segundos = stop.segundos - start.segundos;
	if(start.minutos > stop.minutos){
		--start.hora;
		start.minutos += 60;
	}

	diff->minutos = stop.minutos - start.minutos;
	diff->hora = stop.hora - start.hora;

	diff->dia=stop.dia-start.dia;
	diff->mes=stop.mes-start.mes;
	diff->anio=stop.anio-start.anio;

}



int Iniciar_Programa(tAtributosProg *atributos){

	pthread_attr_t attr;
	pthread_t hilo_prog;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	if(pthread_create(&hilo_prog, &attr, (void *) programa_handler, (void*) atributos) < 0){
		log_error(logger,"No pudo crear hilo.");
		return FALLO_GRAL;
	}
	log_trace(logTrace,"hilo creado");

	return 0;
}

int Finalizar_Programa(int pid){

	tPackHeader recv_head;
	int stat;
	int pack_size;
	char *pid_serial;
	tPackPID pack_pid;

	pack_pid.head.tipo_de_proceso = CON; pack_pid.head.tipo_de_mensaje = KILL_PID;
	pack_pid.val = pid;

	pack_size = 0;

	if ((pid_serial = serializeVal(&pack_pid, &pack_size)) == NULL){
		puts("No se serializo bien");
		return FALLO_SERIALIZAC;
	}

	if ((stat = send(sock_kern, pid_serial, pack_size, 0)) == -1){
		perror("Fallo envio de PID a Consola. error");
		return FALLO_SEND;
	}
	printf("Se enviaron %d bytes al kernel\n", stat);

	free(pid_serial);


	//habria q  ver si importa que el kernel conteste o matarlo de una y fue..
	accionesFinalizacion(pid);

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
void accionesFinalizacion(int pid ){
	int i;
	char buffInicio[100];
    char buffFin[100];
	for(i = 0; i < list_size(listaAtributos); i++){
		tAtributosProg *aux = list_get(listaAtributos, i);
		if(aux->pidProg==pid){

			time(&aux->horaFin);
			double diferencia = difftime(aux->horaFin, aux->horaInicio);

			strftime (buffInicio, 100, "%Y-%m-%d %H:%M:%S.000", localtime (&aux->horaInicio));
			strftime (buffFin, 100, "%Y-%m-%d %H:%M:%S.000", localtime (&aux->horaFin));

			printf("\n\n\n FIN DE LA EJECUCION DEL PROCESO %d \n INFO DE EJECUCION: \n",pid);

			printf ("HORA DE INICIO: %s\n", buffInicio);
			printf ("HORA DE FIN: %s\n", buffFin);
			printf("SEGUNDOS DE EJECUCION: %.f segundos \n",diferencia);
			printf("Cantidad de impresiones por pantalla: XXXXXXX\n");



			log_info(logger,"SEGUNDOS DE EJECUCION: %.f",diferencia);



			//Lo sacamos de la lista de programas en ejecucion
			pthread_mutex_lock(&mux_listaAtributos);
			list_remove(listaAtributos,i);
			pthread_mutex_unlock(&mux_listaAtributos);

			//Lo agregamos a la lista de programas finalizados.
			pthread_mutex_lock(&mux_listaFinalizados);
			list_add(listaFinalizados,aux);
			pthread_mutex_unlock(&mux_listaFinalizados);

			pthread_cancel(aux->hiloProg);

			log_trace(logTrace,"hilo del programa %d finalizado",pid);
		}
	}
}

void Desconectar_Consola(tConsola *cons_data){
	log_trace(logTrace,"eligio desconectar la consola");

	int i;

	for(i = 0; i < list_size(listaAtributos); i++){
		tAtributosProg *aux = list_get(listaAtributos, i);
		pthread_cancel(aux->hiloProg);

		//pthread_kill(aux->hiloProg, SIGDISCONNECT);
	}
	log_trace(logTrace,"todos los hilos cancelados");
}


void Limpiar_Mensajes(){
	log_trace(logTrace,"limpiar mensajes");
	system("clear");
}

void *programa_handler(void *atributos){


	char *buffer;
	char buffInicio[100];
	int pack_size;
	int motivoFin=1;
	int stat;
	int fin = 0;


	log_trace(logTrace,"conectando con kernel");


	sock_kern = establecerConexion(cons_data->ip_kernel, cons_data->puerto_kernel);
	if (sock_kern < 0){
		errno = FALLO_CONEXION;
		log_error(logger,"no se pudo estalecer conexion con kernel.");
		return NULL;
	}


	tAtributosProg *args = (tAtributosProg *) atributos;
	time(&args->horaInicio);
	log_info(logger,"<-- hora inicio");
	strftime (buffInicio, 100, "%Y-%m-%d %H:%M:%S.000", localtime (&args->horaInicio));
	printf ("Hora de inicio: %s\n", buffInicio);

	handshakeCon(sock_kern, CON);
	log_trace(logTrace,"handshake realizado");
	tPackHeader head_tmp;

	log_trace(logTrace,"creando codigo fuente");


	tPackSrcCode *src_code = readFileIntoPack(CON, args->path);
	log_trace(logTrace,"codigo fuente creado");
	log_trace(logTrace,"serializando codigo fuente");
	head_tmp.tipo_de_proceso = CON; head_tmp.tipo_de_mensaje = SRC_CODE; pack_size = 0;
	buffer = serializeBytes(head_tmp, src_code->bytes, src_code->bytelen, &pack_size);

	log_trace(logTrace,"codigo fuente serializado");
	log_trace(logTrace,"enviando codigo fuente");

	if ((stat = send(sock_kern, buffer, pack_size, 0)) == -1){
		log_error(logger,"no se pudo enviar codigo fuente a kernel. ");
		return (void *) FALLO_SEND;
	}
	log_info(logger,"se enviaron %d bytes",stat);


	// enviamos el codigo fuente, lo liberamos ahora antes de olvidarnos..
	freeAndNULL((void **) &src_code->bytes);
	freeAndNULL((void **) &src_code);
	freeAndNULL((void **) &buffer);

	tPackPID *ppid;

	log_trace(logTrace,"esperando a recibir el PID");

	while((stat = recv(sock_kern, &(head_tmp), HEAD_SIZE, 0)) > 0){

		if (head_tmp.tipo_de_mensaje == IMPRIMIR){
			log_trace(logTrace,"kernel manda algo a imprimir");

			log_trace(logTrace, "recibimos info para imprimir");

			if ((buffer = recvGeneric(sock_kern)) == NULL){
				log_error(logger,"error al recibir la info a imprimir");
				return (void *) FALLO_RECV;
			}




			//puts("Kernel manda algo a imprimir");
			//recv..
		}



		if (head_tmp.tipo_de_mensaje == PID){
			log_trace(logTrace,"recibimos PID");

			if ((buffer = recvGeneric(sock_kern)) == NULL){
				log_error(logger,"error al recibir el pid");
				return (void *) FALLO_RECV;
			}

			if ((ppid = deserializeVal(buffer)) == NULL){
				log_error(logger,"error al deserializar el packPID");
				return (void *) FALLO_DESERIALIZAC;
			}

			log_trace(logTrace,"asigno pid a la estructura");

			memcpy(&args->pidProg, &ppid->val, sizeof(int));
			args->hiloProg = pthread_self();
			freeAndNULL((void **)&ppid);






			log_info(logger,"PID: %d ",args->pidProg);

			printf("Pid Recibido es: %d\n", args->pidProg);



			pthread_mutex_lock(&mux_listaAtributos);
			list_add(listaAtributos,args);
			pthread_mutex_unlock(&mux_listaAtributos);

		}


		if (head_tmp.tipo_de_mensaje == FIN_PROG){


			log_trace(logTrace,"se finaliza el hilo");

			log_trace(logTrace,"Kernel nos avisa que termino de ejecutar el programa de manera NORMAL",args->pidProg);

			printf("Kernel nos avisa que termino de ejecutar el programa %d\n", args->pidProg);
			stat = recv(sock_kern, &(head_tmp), HEAD_SIZE, 0);
			motivoFin = head_tmp.tipo_de_mensaje;

			printf("Exit code %d\n",motivoFin);

			accionesFinalizacion(args->pidProg);
			puts("aca");
		}

		if(head_tmp.tipo_de_mensaje == ABORTO_PROCESO){
			log_trace(logTrace,"se finaliza el hilo");

			log_trace(logTrace,"Kernel nos avisa que termino de ejecutar el programa de manera ANORMAL",args->pidProg);
			stat = recv(sock_kern, &(head_tmp), HEAD_SIZE, 0);
			motivoFin = head_tmp.tipo_de_mensaje;

			printf("Exit code %d\n",motivoFin);

			accionesFinalizacion(args->pidProg);
			puts("aca");
		}

	}


	if (stat == -1){
		log_error(logger,"fallo recepcion header en thread de programa");
		return (void *) FALLO_RECV;
	}
	log_trace(logTrace,"kernel cerro conexion con thread de programa");
	return NULL;
}













