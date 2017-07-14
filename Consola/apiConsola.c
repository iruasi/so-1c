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
#define MUTEX 1

int siguientePID(void){return 1;}
int tamanioPrograma;
int sock_kern;
extern t_list *listaAtributos,*listaFinalizados;
extern t_log * logger;
sem_t semLista;
extern tConsola *cons_data;


void inicializarSemaforos(void){
	int stat;
	if ((stat = sem_init(&semLista, 0, MUTEX)) == -1)
		log_error(logger,"No se pudo inicializar semaforo lista.");
	//perror("No se pudo inicializar semaforo lista. error");

}

void eliminarSemaforos(void){
	sem_destroy(&semLista);
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
		//perror("No pudo crear hilo. error");
		return FALLO_GRAL;
	}
	log_trace(logger,"hilo creado");
	//puts("hilo creado");

	return 0;
}

int Finalizar_Programa(int pid){

	tPackHeader send_head, recv_head;
	send_head.tipo_de_mensaje = KILL_PID;
	send_head.tipo_de_proceso = CON;
	tPackPID *ppid = malloc(sizeof *ppid);
	ppid->head = send_head;
	ppid->val=pid;

	int stat;
	if((stat = send(sock_kern, ppid, sizeof ppid, 0)) == -1){
		log_error(logger,"Fallo el envio de pid a matar a Kernel.");
		//perror("Fallo el envio de pid a matar a Kernel. error");
		return FALLO_SEND;
	}


	if((stat = recv(sock_kern, &recv_head, sizeof recv_head, 0)) == -1){
		log_error(logger,"Fallo de recepcion respuesta Kernel");
		//perror("Fallo de recepcion respuesta Kernel. error");
		return FALLO_RECV;

	} else if (stat == 0){
		log_trace(logger,"Kernel cerro la conexion, antes de dar una respuesta...");
		//puts("Kernel cerro la conexion, antes de dar una respuesta...");
		return FALLO_GRAL;
	}

	if (recv_head.tipo_de_mensaje != KER_KILLED){
		log_info(logger,"No se pudo matar (o no se recibio la confirmacion)");
		//puts("No se pudo matar (o no se recibio la confirmacion)");
		return FALLO_MATAR;
	}

	//TODO: Arriba kernel nos avisa si mato el proceso, pero si no pudo matar, este hilo tmb hay q volarlo?
	//por ahora lo vuelo
	int i;

	for(i = 0; i < list_size(listaAtributos); i++){
		tAtributosProg *aux = list_get(listaAtributos, i);
		if(aux->pidProg==pid){

			pthread_cancel(aux->hiloProg);
			log_trace(logger,"hilo del programa finalizado");

			//pthread_kill(aux->hiloProg, SIGDISCONNECT);


			//printf("\nHilo: %u finalizado crrectamente\n",aux->hiloProg);

			log_trace(logger,"hilo finalizado");

			time(&aux->horaFin);


			double diferencia = difftime(aux->horaFin, aux->horaInicio);
			printf("Segs de ejecucion: %.f segundos \n",diferencia);
			//Lo sacamos de la lista de programas en ejecucion

			list_remove(listaAtributos,i);

			//Lo agregamos a la lista de programas finalizados.
			list_add(listaFinalizados,aux);
		}
	}

	return 0;
}

void Desconectar_Consola(tConsola *cons_data){
	log_trace(logger,"eligio desconectar la consola");
	//puts("Eligio desconectar esta consola !\n");
	int i;

	for(i = 0; i < list_size(listaAtributos); i++){
		tAtributosProg *aux = list_get(listaAtributos, i);
		pthread_cancel(aux->hiloProg);
		//pthread_kill(aux->hiloProg, SIGDISCONNECT);
	}
	log_trace(logger,"todos los hilos cancelados");
}


void Limpiar_Mensajes(){
	log_trace(logger,"limpiar mensajes");
	system("clear");
}

void *programa_handler(void *atributos){

	log_trace(logger,"conectando con kernel");
	//printf("Conectando con kernel...\n");

	sock_kern = establecerConexion(cons_data->ip_kernel, cons_data->puerto_kernel);
	if (sock_kern < 0){
		errno = FALLO_CONEXION;
		log_error(logger,"no se pudo estalecer conexion con kernel.");
		//perror("No se pudo establecer conexion con Kernel. error");
		return NULL;
	}

	char *buffer;
	int pack_size;
	tAtributosProg *args = (tAtributosProg *) atributos;
	time(&args->horaInicio);
	log_info(logger,"<-- hora inicio");
	int stat;

	handshakeCon(sock_kern, CON);
	//puts("handshake realizado");
	log_trace(logger,"handshake realizado");
	tPackHeader head_tmp;

	log_trace(logger,"creando codigo fuente");

	//puts("Creando codigo fuente...");

	tPackSrcCode *src_code = readFileIntoPack(CON, args->path);
	log_trace(logger,"codigo fuente creado");
	//puts("codigo fuente creado");
	log_trace(logger,"serializando codigo fuente");
	//puts("Serializando codigo fuente...");
	head_tmp.tipo_de_proceso = CON; head_tmp.tipo_de_mensaje = SRC_CODE; pack_size = 0;
	buffer = serializeBytes(head_tmp, src_code->bytes, src_code->bytelen, &pack_size);

	log_trace(logger,"codigo fuente serializado");
	//puts("codigo fuente serializado");
	puts("Enviando codigo fuente...");
	if ((stat = send(sock_kern, buffer, pack_size, 0)) == -1){
		log_error(logger,"no se pudo enviar codigo fuente a kernel. ");
		//perror("No se pudo enviar codigo fuente a Kernel. error");
		return (void *) FALLO_SEND;
	}
	log_info(logger,"se enviaron %d bytes","stat");
	//printf("Se enviaron %d bytes..\n", stat);


	// enviamos el codigo fuente, lo liberamos ahora antes de olvidarnos..
	freeAndNULL((void **) &src_code->bytes);
	freeAndNULL((void **) &src_code);
	freeAndNULL((void **) &buffer);

	tPackPID *ppid;
	log_trace(logger,"esperando a recibir el PID");
	//puts("Esperando a recibir el PID");
	int fin = 0;

	while(fin !=1){
		while((stat = recv(sock_kern, &(head_tmp), HEAD_SIZE, 0)) > 0){

			if (head_tmp.tipo_de_mensaje == IMPRIMIR){
				log_trace(logger,"kernel manda algo a imprimir");
				//puts("Kernel manda algo a imprimir");
				//recv..
			}

			if (head_tmp.tipo_de_mensaje == PID){
				log_trace(logger,"reciimos PID");
				//puts("recibimos PID");

				if ((buffer = recvGeneric(sock_kern)) == NULL){
					log_error(logger,"error al recibir el pid");
					return (void *) FALLO_RECV;
				}

				if ((ppid = deserializeVal(buffer)) == NULL){
					log_error(logger,"error al deserializar el packPID");
					return (void *) FALLO_DESERIALIZAC;
				}

				log_trace(logger,"asigno pid a la estructura");
				//puts("Asigno pid a la estructura");
				memcpy(&args->pidProg, &ppid->val, sizeof(int));
				log_info(logger,"El pid asignado a la estructura es: %d",args->pidProg);
				args->hiloProg = pthread_self();
				freeAndNULL((void **)&ppid);

				sem_wait(&semLista);
				list_add(listaAtributos,args);
				sem_post(&semLista);

				log_info(logger,"PID: &d""args->pidProg");
				//printf(" pid %d\n",args->pidProg);

			}


			if (head_tmp.tipo_de_mensaje == FIN_PROG){
				log_trace(logger,"se finaliza el hilo");
				//puts("Se finaliza el hilo");
				pthread_cancel(pthread_self());
			}
		}
	}

	if (stat == -1){
		log_error(logger,"fallo recepcion header en thread de programa");
		//perror("Fallo recepcion header en thread de programa. error");
		return (void *) FALLO_RECV;
	}
	log_trace(logger,"kernel cerro conexion con thread de programa");
	//puts("Kernel cerro conexion con thread de programa");
	return NULL;
}













