#ifndef APICONSOLA_H_
#define APICONSOLA_H_

#include <tiposRecursos/tiposPaquetes.h>

typedef struct {
	int sock;
	char *path;
	pthread_t hiloProg;
	int pidProg;
} tAtributosProg;




int Iniciar_Programa(tAtributosProg *pack);


int Finalizar_Programa(int process_id, int sock_kernel, pthread_attr_t attr);


void Desconectar_Consola();


void Limpiar_Mensajes();


void *programa_handler(void *argsX);


int agregarAListaDeProgramas(int process_id);

#endif // APICONSOLA_H_
