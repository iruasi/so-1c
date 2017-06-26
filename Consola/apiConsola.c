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

#define SIGDISCONNECT 80
#define MUTEX 1

int siguientePID(void){return 1;}
int tamanioPrograma;
extern t_list *listaAtributos;
sem_t *semLista;
extern tConsola *cons_data;

void inicializarSemaforos(void){
	int stat;
	semLista = malloc(sizeof (sem_t));
	 if ((stat = sem_init(semLista, 0, MUTEX)) == -1)
		perror("No se pudo inicializar semaforo lista. error");

}

void eliminarSemaforos(void){
	sem_destroy(semLista);
}

int Iniciar_Programa(tAtributosProg *atributos){

	int stat, *retval;

	handshakeCon(atributos->sock, CON);
	puts("handshake realizado");

	pthread_attr_t attr;
	pthread_t hilo_prog;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	if(pthread_create(&hilo_prog, &attr, (void *) programa_handler, (void*) atributos) < 0){
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
		return FALLO_GRAL;programa_handler
	}

	if (recv_head.tipo_de_mensaje != KER_KILLED){
		puts("No se pudo matar (o no se recibio la confirmacion)");
		return FALLO_MATAR;
	}

	pthread_attr_destroy(&attr);
	return 0;
}

void Desconectar_Consola(tConsola *cons_data){
	puts("Eligio desconectar esta consola !\n");
	int i;

	for(i = 0; i < list_size(listaAtributos); i++){
		tAtributosProg *aux = list_get(listaAtributos, i);
		pthread_kill(aux->hiloProg, SIGDISCONNECT);
	}
}


void Limpiar_Mensajes(){
	system("clear");
}

void *programa_handler(void *atributos){

	printf("Conectando con kernel...\n");
	int sock_kern = establecerConexion(cons_data->ip_kernel, cons_data->puerto_kernel);
	if (sock_kern < 0){
		errno = FALLO_CONEXION;
		perror("No se pudo establecer conexion con Kernel. error");
		return NULL;
	}

	tAtributosProg *args = (tAtributosProg *) atributos;
	int stat;

	handshakeCon(sock_kern, CON);
	puts("handshake realizado");
	tPackHeader head_tmp;

	puts("Creando codigo fuente...");

	tPackSrcCode *src_code = readFileIntoPack(CON, args->path);
	puts("codigo fuente creado");
	puts("Serializando codigo fuente...");
	tPackSrcCode *paquete_serializado = serializarSrcCode(src_code);
	puts("codigo fuente serializado");
	puts("Enviando codigo fuente...");
	int packSize = sizeof src_code->head + sizeof src_code->sourceLen + src_code->sourceLen;
	if ((stat = send(sock_kern, paquete_serializado, packSize, 0)) < 0){
		perror("No se pudo enviar codigo fuente a Kernel. error");
		return (void *) FALLO_SEND;
	}
	printf("Se enviaron %d bytes..\n", stat);


	// enviamos el codigo fuente, lo liberamos ahora antes de olvidarnos..
	freeAndNULL((void **) &src_code->sourceCode);
	freeAndNULL((void **) &src_code);
	freeAndNULL((void **) &paquete_serializado);

	tPackPID ppid;
	ppid.head = head_tmp;
	puts("Esperando a recibir el PID");
	int fin = 0;


while(fin !=1){
	while((stat = recv(args->sock, &(head_tmp), HEAD_SIZE, 0)) > 0){

		if (head_tmp.tipo_de_mensaje == PID){
			puts("recibimos PID");
			stat = recv(args->sock, &(ppid.pid), sizeof ppid.pid, 0);
			puts("Asigno pid a la estructura");
			args->pidProg = ppid.pid;
			args->hiloProg = pthread_self();

			sem_wait(semLista);
			list_add(listaAtributos,args);
			sem_post(semLista);

			printf(" pid %d\n",args->pidProg);

//			printf(" hilo %d\n",args->hiloProg);
			/*int program_stat;
			puts("agregamos el programa a la lista de progamas");

			program_stat = agregarAListaDeProgramas(ppid.pid);

			if(program_stat != 1){
				puts("Error al agregar el programa");
				//TODO: ADD_PROGRAM_ERROR
				return -99;
			}*/
		}
		//todo: aca vendria otro if cuando kernel tiene q imprimir algo por consola. le manda un mensaje.
		/*if(head_tmp.tipo_de_mensaje == PRINT){
			puts("Recibimos info para imprimir");
			stat = recv(args->sock,Tamanoaimprimir,sizeof(int),0);
			stat = recv(args->sock,mensaje,tamanoAImprimir,0);
			puts(mensaje);
		}*/

		if (head_tmp.tipo_de_mensaje == FIN_PROG){


			puts("Se finaliza el hilo");
			pthread_exit(NULL);
		}
	}
}

	if (stat == -1){
		perror("Fallo recepcion header en thread de programa. error");
		return (void *) FALLO_RECV;
	}

	puts("Kernel cerro conexion con thread de programa");
	return NULL;
}













