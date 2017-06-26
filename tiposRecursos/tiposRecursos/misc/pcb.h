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
	int id;
	posicionMemoria pos;
}posicionMemoriaId;



typedef struct{
	t_list *args; // Posiciones de memoria de las copias de los argumentos de la función.
	t_list *vars; // Identificadores y posiciones de memoria de las variables locales.
	int retPos; // Posicion del indice de codigo donde retorna al finalizar la funcion.
	posicionMemoria retVar; // Posicion de memoria para guardar el resultado
}indiceStack;

typedef struct {
	int		 id, // PID
			 pc, // Program Counter
			 paginasDeCodigo, // paginas de codigo
			 etiquetaSize,
			 cantidad_instrucciones,
			 estado_proc, // refleja en que cola de Planificador esta el PCB
			 contextoActual, // para pasaje de una funcion a otra todo: mejorar descripcion jaja
			 exitCode;
	t_intructions* indiceDeCodigo; //El t_instructions es del parser ansisop
	t_list* indiceDeStack;
	char* indiceDeEtiquetas;

}__attribute__((packed)) tPCB; //https://www.google.com.ar/search?q=__attribute__%28%28packed%29%29+tad+C&ie=utf-8&oe=utf-8&client=firefox-b-ab&gfe_rd=cr&ei=q9k5WcLfC4rX8geiq6CQBQ
#define CTES_INT_PCB 8 // usamos este #define para auxiliar la mantenibilidad del PCB y funciones que le conciernen

#endif /* PCB_H_ */
