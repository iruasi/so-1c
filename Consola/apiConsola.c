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

void guardarHoraActual(tHora *horaActual){

	time_t rawtime;
	struct tm * timeinfo;

	time ( &rawtime );
	timeinfo = localtime ( &rawtime );

	printf ( "Current local time and date: %s", asctime (timeinfo) );

	horaActual->anio=timeinfo->tm_year;
	horaActual->mes=timeinfo->tm_mon;
	horaActual->dia=timeinfo->tm_mday;
	horaActual->hora=timeinfo->tm_hour;
	horaActual->minutos=timeinfo->tm_min;
	horaActual->segundos=timeinfo->tm_sec;



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

	int stat, *retval;



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

int Finalizar_Programa(int pid, int sock_ker){

	tPackHeader send_head, recv_head;
	send_head.tipo_de_mensaje = KILL_PID;
	send_head.tipo_de_proceso = CON;
	tPackPID *ppid = malloc(sizeof *ppid);
	ppid->head = send_head;
	ppid->val=pid;

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

	int i;

	for(i = 0; i < list_size(listaAtributos); i++){
		tAtributosProg *aux = list_get(listaAtributos, i);
		if(aux->pidProg==pid){
			pthread_cancel(aux->hiloProg);
			//pthread_kill(aux->hiloProg, SIGDISCONNECT);


			printf("\nHilo: %u finalizado crrectamente\n",aux->hiloProg);
			printf("Inicio: ");
			printf("Fecha inicio: %d/%d/%d\n",aux->horaInicio.anio,aux->horaInicio.mes,aux->horaInicio.dia);
			printf("Hora: %d:%d:%d\n",aux->horaInicio.hora,aux->horaInicio.minutos,aux->horaInicio.segundos);


			/*tHora diff;
			differenceBetweenTimePeriod(aux->horaInicio,aux->horaFin,&diff);
			printf("Tiempo de ejecucion: %d Anio(s) \n %d Mes(es) \n %d dia(s) \n",diff.anio,diff.mes,diff.dia);
			printf("%d Hora(s) \n %d Minuto(s) \n %d Segundo(s) \n",diff.hora,diff.minutos,diff.segundos);
			*/


			time_t rawtime;
			struct tm * timeinfo;

			time ( &rawtime );
			timeinfo = localtime ( &rawtime );

			struct tm *timeInicio;
		//	timeInicio=localtime(&aux->hi);
			printf("Fecha y hora inicio: %s",asctime(timeInicio));
			printf ( "Fecha y hora fin: %s", asctime (timeinfo) );


			//time(&aux->hf);


			//double diferencia = difftime(aux->hf, aux->hi);
			//printf("Segs de ejecucion: %.f segundos \n",diferencia);
			//Lo sacamos de la lista de programas en ejecucion
			list_remove(listaAtributos,i);

			//Lo agregamos a la lista de programas finalizados.
			//list_add(listaFinalizados,aux);
		}
	}

	return 0;
}

void Desconectar_Consola(tConsola *cons_data){
	puts("Eligio desconectar esta consola !\n");
	int i;

	for(i = 0; i < list_size(listaAtributos); i++){
		tAtributosProg *aux = list_get(listaAtributos, i);
		pthread_cancel(aux->hiloProg);
		//pthread_kill(aux->hiloProg, SIGDISCONNECT);
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
	while((stat = recv(sock_kern, &(head_tmp), HEAD_SIZE, 0)) > 0){

		if (head_tmp.tipo_de_mensaje == PID){
			puts("recibimos PID");

			if((stat = recv(sock_kern, &(ppid.val), sizeof ppid.val, 0)) < 0 ){
				perror("error al recibir el pid");
				return 0;
			}

			puts("Asigno pid a la estructura");
			args->pidProg = ppid.val;
			args->hiloProg = pthread_self();

			//sem_wait(semLista);
			list_add(listaAtributos,args);
			//sem_post(semLista);

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
			//pthread_exit(NULL);
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













