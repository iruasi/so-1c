#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <commons/collections/list.h>
#include <commons/log.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "apiConsola.h"
#include "auxiliaresConsola.h"
#include "consolaConfigurators.h"

#include <tiposRecursos/tiposErrores.h>
#include <tiposRecursos/tiposPaquetes.h>
#include <funcionesPaquetes/funcionesPaquetes.h>
#include <funcionesCompartidas/funcionesCompartidas.h>

#ifndef HEAD_SIZE
#define HEAD_SIZE 8
#endif

extern t_log*logger;

int siguientePID(void){return 1;}
int tamanioPrograma;
int Iniciar_Programa(tPathYSock *args){

	int stat, *retval;

	handshakeCon(args->sock, CON);
	log_info(logger,"handshake realizado");

	pthread_attr_t attr;
	pthread_t hilo_prog;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	if(pthread_create(&hilo_prog, &attr, (void *) programa_handler, (void*) args) < 0){
		log_error(logger,"No pudo crear hilo. error");
		return FALLO_GRAL;
	}

	log_info(logger,"hilo creado");

	if ((stat = pthread_join(hilo_prog, &retval)) < 0){
		log_error(logger, "pthread joineo de forma erronea. status: %d\n", stat);
		return FALLO_HILO_JOIN;
	}

	log_info(logger,"El hilo retorno con valor retorno: %d\n", *retval);
	return 0;
}

int Finalizar_Programa(int pid, int sock_ker, pthread_attr_t attr){

	tPackHeader send_head, recv_head;
	send_head.tipo_de_mensaje = KILL_PID;
	send_head.tipo_de_proceso = CON;
	tPackPID *ppid = malloc(sizeof *ppid);
	ppid->head = send_head;
	ppid->pid  = pid;

	int stat;
	if((stat = send(sock_ker, ppid, sizeof ppid, 0)) == -1){
		log_error(logger,"Fallo el envio de pid a matar a Kernel");
		return FALLO_SEND;
	}


	if((stat = recv(sock_ker, &recv_head, sizeof recv_head, 0)) == -1){
		log_error(logger,"Fallo de recepcion respuesta Kernel");
		return FALLO_RECV;

	} else if (stat == 0){
		log_error(logger,"Kernel cerro la conexion, antes de dar una respuesta...");
		return FALLO_GRAL;
	}

	if (recv_head.tipo_de_mensaje != KER_KILLED){
		log_error(logger,"No se pudo matar (o no se recibio la confirmacion)");
		return FALLO_MATAR;
	}

	pthread_attr_destroy(&attr);
	return 0;
}

// todo: depende de como tengamos almacenados los PIDs (con attributos, o algunoa otra info)
// segun eso, depende la cantidad de parametros de esta funcion etc...
void Desconectar_Consola(int sock_ker, pthread_attr_t attr, tConsola *cons_data){

	int pid;

	while((pid = siguientePID())){
		Finalizar_Programa(pid, sock_ker, attr);
	}

	close(sock_ker);
	liberarConfiguracionConsola(cons_data);
}


void Limpiar_Mensajes(){
	system("clear");
}

void *programa_handler(void *pathYSock){

	tPathYSock *args = (tPathYSock *) pathYSock;
	int stat;

	tPackHeader head_tmp;

	log_info(logger,"Creando codigo fuente...");

	tPackSrcCode *src_code = readFileIntoPack(CON, args->path);
	log_info(logger,"codigo fuente creado");
	log_info(logger,"Serializando codigo fuente...");
	tPackSrcCode *paquete_serializado = serializarSrcCode(src_code);
	log_info(logger,"codigo fuente serializado");
	log_info(logger,"Enviando codigo fuente...");
	int packSize = sizeof src_code->head + sizeof src_code->sourceLen + src_code->sourceLen;
	if ((stat = send(args->sock, paquete_serializado, packSize, 0)) < 0){
		log_error(logger,"No se pudo enviar codigo fuente a Kernel");
		return (void *) FALLO_SEND;
	}
	log_info(logger,"Se enviaron %d bytes..\n", stat);


	// enviamos el codigo fuente, lo liberamos ahora antes de olvidarnos..
	freeAndNULL((void **) &src_code->sourceCode);
	freeAndNULL((void **) &src_code);
	freeAndNULL((void **) &paquete_serializado);

	tPackPID ppid;
	ppid.head = head_tmp;
	log_info(logger,"Esperando a recibir el PID");
	while((stat = recv(args->sock, &(head_tmp), HEAD_SIZE, 0)) > 0){

		if (head_tmp.tipo_de_mensaje == RECV_PID){
			log_info(logger,"recibimos PID");
			stat = recv(args->sock, &(ppid.pid), sizeof ppid.pid, 0);
			int program_stat;
			log_info(logger,"agregamos el programa a la lista de progamas");

			program_stat = agregarAListaDeProgramas(ppid.pid);

			if(program_stat != 1){
				log_info(logger,"Error al agregar el programa");
				//TODO: ADD_PROGRAM_ERROR
				return -99;
			}
		}
		//todo: aca vendria otro if cuando kernel tiene q imprimir algo por consola. le manda un mensaje.
		/*if(head_tmp.tipo_de_mensaje == PRINT){
			puts("Recibimos info para imprimir");
			stat = recv(args->sock,Tamanoaimprimir,sizeof(int),0);
			stat = recv(args->sock,mensaje,tamanoAImprimir,0);
			puts(mensaje);
		}*/
	}

	if (stat == -1){
		log_error(logger,"Fallo recepcion header en thread de programa. error");
		return (void *) FALLO_RECV;
	}

	log_info(logger,"Kernel cerro conexion con thread de programa");
	return NULL;
}












