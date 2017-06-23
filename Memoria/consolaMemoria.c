#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "apiMemoria.h"

/* Con este macro verificamos igualdad de strings;
 * es mas expresivo que strcmp porque devuelve true|false mas humanamente
 */
#define STR_EQ(BUF, CC) (!strcmp((BUF),(CC)))

#define CHAR_ESPUREO(C) ((C == '\0' || C == '\n' || C == '\t' || C == '\r')? true : false)

void accionarMenu(void);
int obtenerOpcionDump(void);
void mostrarMenu(void);

// CONSOLA DE LA MEMORIA

void *consolaUsuario(void){

	accionarMenu();
	// bla ble bli
	return NULL;
}

// todo: mejorar forma de recibir inputs para que no se rompa de un soplo.
//       es decir, por ahora el scanf toma el '\n' de lo que se ingrese,
//       y eso es problematico...
void accionarMenu(void){

	int lag, pid;
	char ch[3] = "u\n\0";

	while(ch != NULL){
		mostrarMenu();

		fgets(ch, 3, stdin);

		printf("Lo que mandaste fue %s..\n", ch);

		if (STR_EQ(ch, "r\n\0")){

			printf("Ingrese una latencia de accesos a Memoria en milisegundos \n > ");
			scanf("%d", &lag);
			if (lag <= 0){
				printf("%d no es una latencia valida.\n", lag);
				break;
			}

			retardo(lag);
			continue;
			puts("fe");
		}

		else if (STR_EQ(ch, "d\n\0")){

			dump(obtenerOpcionDump());
			break;
		}

		else if (STR_EQ(ch, "f\n\0")){
			puts("Se limpia la CACHE...");
			flush();
			puts("CACHE limpiada!");
			break;
		}

		else if (STR_EQ(ch, "s\n\0")){

			scanf("Ingrese el PID del cual quiere conocer su tamanio (negativo para Memoria).\n > %d", &pid);
			size(pid);
			break;
		}

		else{
			puts("Opcion invalida");
		}

		puts("fa");
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
