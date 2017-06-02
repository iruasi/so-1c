#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>

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

int siguientePID(void){return 1;}

int Iniciar_Programa(tPathYSock *args){

	handshakeCon(args->sock, CON);
	puts("hs realizado");
	printf("hs realizado");
	pthread_attr_t attr;
	pthread_t hilo_prog;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	if(pthread_create(&hilo_prog, &attr, (void *) programa_handler, (void*) args) < 0){
		perror("No pudo crear hilo. error");
		return FALLO_GRAL;
	}

	puts("hilo creado");

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
		perror("Fallo el envio de pid a matar a Kernel. error");
		return FALLO_SEND;
	}


	if((stat = recv(sock_ker, &recv_head, sizeof recv_head, 0)) == -1){
		perror("Fallo de recepcion respuesta Kernel. error");
		return FALLO_RECV;

	} else if (stat == 0){
		puts("Kernel cerro la conexion, antes de dar una respuesta...");
		return FALLO_GRAL;
	}

	if (recv_head.tipo_de_mensaje != KER_KILLED){
		puts("No se pudo matar (o no se recibio la confirmacion)");
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


void Limpiar_Mensajes(int cons_height){
	int i;
	for (i = 0; i < cons_height; i++)
		puts("");
}

void *programa_handler(void *pathYSock){

	tPathYSock *args = (tPathYSock *) pathYSock;
	int stat;
	tPackHeader head_tmp;

	puts("Creando codigo fuente...");
	tPackSrcCode *src_code = readFileIntoPack(CON, args->path);
	puts("codigo fuente creado");
	puts("Serializando codigo fuente...");
	void *paquete_serializado = serializarSrcCode(src_code);
	puts("codigo fuente serializado");
	puts("Enviando codigo fuente...");
	int packSize = sizeof src_code->head + sizeof src_code->sourceLen + src_code->sourceLen;
	if ((stat = send(args->sock, paquete_serializado, packSize, 0)) < 0){
		perror("No se pudo enviar codigo fuente a Kernel. error");
		return (void *) FALLO_SEND;
	}

	printf("Se envio el paquete de codigo fuente...");
	// enviamos el codigo fuente, lo liberamos ahora antes de olvidarnos..
	freeAndNULL(src_code->sourceCode);
	freeAndNULL(src_code);
	freeAndNULL(paquete_serializado);

	tPackPID ppid;
	ppid.head = head_tmp;

	while((stat = recv(args->sock, &ppid.head, HEAD_SIZE, 0)) != -1){

		if (head_tmp.tipo_de_mensaje == RECV_PID){
			puts("recibimos PID");
			stat = recv(args->sock, &ppid.pid, sizeof ppid.pid, 0);
			//todo: aca hay q agregar el id del proceso q acabamos de crear a una lista o algo asi para poder manejarlo.
			//agregarPrograma();
		}
		//todo: aca vendria otro if cuando kernel tiene q imprimir algo por consola. le manda un mensaje.
		/*if(head_tmp.tipo_de_mensaje == PRINT){
			puts("Recibimos info para imprimir");
			stat = recv(args->sock,Tamanoaimprimir,sizeof(int),0);
			stat = recv(args->sock,mensaje,tamanoAImprimir,0);
			puts(mensaje);
		}*/




		puts("Recibimos info para imprimir");
	}

	if (stat == -1){
		perror("Fallo recepcion header en thread de programa. error");
		return (void *) FALLO_RECV;
	}

	puts("Kernel cerro conexion con thread de programa");
	return NULL;
}
