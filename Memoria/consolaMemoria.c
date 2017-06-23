#include <stdio.h>

#include "apiMemoria.h"


void accionarMenu(void);
int obtenerOpcionDump(void);
void mostrarMenu(void);

// CONSOLA DE LA MEMORIA

void *consolaUsuario(void){

	accionarMenu();
	return NULL;
}

void accionarMenu(void){

	int lag, pid;
	char ch[20] = "a";

	while(ch != NULL){
		mostrarMenu();
		gets();

		printf("Lo que mandaste fue %ud\ny\n%c", ch, ch);

		switch(ch){

		case 'r':

			scanf("Ingrese una latencia de accesos a Memoria en milisegundos \n > %d", &lag);
			if (lag <= 0){
				printf("%d no es una latencia valida.\n", lag);
				break;
			}

			retardo(lag);
			break;

		case 'd':

			dump(obtenerOpcionDump());
			break;

		case 'f':
			puts("Se limpia la CACHE...");
			flush();
			puts("CACHE limpiada!");
			break;

		case 's':

			scanf("Ingrese el PID del cual quiere conocer su tamanio (negativo para Memoria).\n > %d", &pid);
			size(pid);
			break;

		default:
			puts("Opcion invalida");
		}

	}
}

void mostrarMenu(void){
	puts("\nIngrese su comando.");
	puts("1. [r]etardo");
	puts("2. [d]ump");
	puts("3. [f]lush");
	puts("4. [s]ize");
}

int obtenerOpcionDump(void){
	puts("Quiere dumpear Cache, Tabla de Paginas, o procesos de Memoria?");
	puts("bla ble bli");
	return 0;
}
