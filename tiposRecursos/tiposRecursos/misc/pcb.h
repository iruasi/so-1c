#ifndef PCB_H_
#define PCB_H_

#include <stdint.h>
#include <commons/collections/list.h>
#include <parser/metadata_program.h>


/*
typedef struct{
	uint32_t offsetInicio;
	uint32_t offsetFin;
}indiceCodigo;
 * */

//TODO: ver como armar esta estructura. quizas se solucione con un char* en pcb
typedef struct{

}indiceEtiquetas;

typedef struct{
	uint32_t pag;
	uint32_t offset;
	uint32_t size;
}posicionMemoria;

typedef struct{
	uint32_t pid;
	posicionMemoria pos;
}posicionMemoriaPid;

typedef struct{
	posicionMemoria* args; // Posiciones de memoria de las copias de los argumentos de la función.
	posicionMemoriaPid* vars; // Identificadores y posiciones de memoria de las variables locales.
	uint32_t retPos; // Posicion del indice de codigo donde retorna al finalizar la funcion.
	posicionMemoria* retVar; // Posicion de memoria para guardar el resultado
}indiceStack;

typedef struct {
	uint32_t id, // PID
			 pc, // Program Counter
			 paginasDeCodigo, // paginas de codigo
			 etiquetaSize,
			 cantidad_instrucciones,
			 exitCode;
	t_intructions* indiceDeCodigo; //El t_instructions es del parser ansisop
	indiceStack* indiceDeStack;
	char* indiceDeEtiquetas;

}__attribute__((packed)) tPCB; //https://www.google.com.ar/search?q=__attribute__%28%28packed%29%29+tad+C&ie=utf-8&oe=utf-8&client=firefox-b-ab&gfe_rd=cr&ei=q9k5WcLfC4rX8geiq6CQBQ

/*
 * typedef struct {
		t_puntero_instruccion	start;
		t_size		offset;
	} t_intructions;

	comento esta estructura para no estar haciendo alt+tab siempre que tengamos que ver como era la estructura
 * */

#endif /* PCB_H_ */
