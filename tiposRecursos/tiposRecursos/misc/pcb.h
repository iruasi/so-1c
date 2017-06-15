#ifndef PCB_H_
#define PCB_H_

#include <stdint.h>
#include <commons/collections/list.h>
#include <parser/metadata_program.h>


typedef struct{
	int pag;
	int offset;
	int size;
}posicionMemoria;

typedef struct{
	int pid;
	posicionMemoria pos;
}posicionMemoriaPid;

typedef struct{
	posicionMemoria* args; // Posiciones de memoria de las copias de los argumentos de la función.
	posicionMemoriaPid* vars; // Identificadores y posiciones de memoria de las variables locales.
	int retPos; // Posicion del indice de codigo donde retorna al finalizar la funcion.
	posicionMemoria* retVar; // Posicion de memoria para guardar el resultado
}indiceStack;

typedef struct {
	int		 id, // PID
			 pc, // Program Counter
			 paginasDeCodigo, // paginas de codigo
			 etiquetaSize,
			 cantidad_instrucciones,
			 exitCode;
	t_intructions* indiceDeCodigo; //El t_instructions es del parser ansisop
	t_list* indiceDeStack;
	char* indiceDeEtiquetas;

}__attribute__((packed)) tPCB; //https://www.google.com.ar/search?q=__attribute__%28%28packed%29%29+tad+C&ie=utf-8&oe=utf-8&client=firefox-b-ab&gfe_rd=cr&ei=q9k5WcLfC4rX8geiq6CQBQ


#endif /* PCB_H_ */
