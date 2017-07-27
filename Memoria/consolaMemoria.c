#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "apiMemoria.h"
#include "consolaMemoria.h"
#include <commons/log.h>

extern t_log*logTrace;
// CONSOLA DE LA MEMORIA

void showOpciones(void){
	printf("\n \n \nIngrese accion a realizar:\n");
	printf ("1-Para Modificar retardo: 'retardo <ms>'\n");
	printf ("2-Para Generar reporte del estado actual: 'dump'\n");
	printf ("3-Para limpiar la cache: 'flush'\n");
	printf ("4-Para ver size de la memoria: 'sizeMemoria'\n");
	printf ("5-Para ver size de un proceso: 'sizeProceso <PID>'\n");

}

void consolaMemoria(void){

	char opcion[MAXOPCION];
	int finalizar = 0;
	showOpciones();
	while(finalizar != 1){
		printf("Seleccione opcion: \n");
		fgets(opcion, MAXOPCION, stdin);
		opcion[strlen(opcion) - 1] = '\0';

		if (strncmp(opcion, "\0", 1) == 0){
			showOpciones();
			continue;
		}

		if (strncmp(opcion, "retardo", 7) == 0){
			puts("Opcion retardo");
			log_trace(logTrace,"opcion retardo");
			char *msChar = opcion + 8;
			int ms = atoi(msChar);
			retardo(ms);

		} else if (strncmp(opcion, "dump", 4) == 0){
			puts("Opcion dump");
			log_trace(logTrace,"opcion dump");
			char *dpChar = opcion + 5;
			int dp = atoi(dpChar);
			dump(dp);

		} else if (strncmp(opcion, "flush", 5) == 0){
			puts("Opcion flush");
			log_trace(logTrace,"opcion flush");
			flush();

		} else if (strncmp(opcion, "sizeMemoria", 11) == 0){
			puts("Opcion sizeMemoria");
			log_trace(logTrace,"opcion size memoria");
			size(-1);

		} else if (strncmp(opcion, "sizeProceso", 11) == 0){
			puts("Opcion sizeProceso");
			log_trace(logTrace,"opcion sizeproceso");
			char *pidProceso = opcion+12;
			int pid = atoi(pidProceso);
			size(pid);
		}
	}
}
