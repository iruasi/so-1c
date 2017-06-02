#ifndef APICONSOLA_H_
#define APICONSOLA_H_

#include <tiposRecursos/tiposPaquetes.h>

typedef struct {
	int sock;
	char *path;
} tPathYSock;



int Iniciar_Programa(tPathYSock *pack);


int Finalizar_Programa(int process_id, int sock_kernel, pthread_attr_t attr);


void Desconectar_Consola();


void Limpiar_Mensajes(int console_height);


void *programa_handler(void *argsX);




#endif // APICONSOLA_H_
