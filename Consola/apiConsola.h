#ifndef APICONSOLA_H_
#define APICONSOLA_H_

#include <tiposRecursos/tiposPaquetes.h>



typedef struct{
	int dia;
	int mes;
	int anio;
	int hora;
	int minutos;
	int segundos;
}tHora;

typedef struct {
	int sock;
	char *path;
	pthread_t hiloProg;
	int pidProg;
	time_t horaInicio;
	time_t horaFin;
	int cantImpresiones;
} tAtributosProg;


int Iniciar_Programa(tAtributosProg *pack);


int Finalizar_Programa(int process_id);


void Desconectar_Consola();


void Limpiar_Mensajes();


void *programa_handler(void *argsX);


void accionesFinalizacion(int pid);
void inicializarSemaforos(void);

int agregarAListaDeProgramas(int process_id);

#endif // APICONSOLA_H_
