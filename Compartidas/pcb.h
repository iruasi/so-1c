#ifndef PCB_H_
#define PCB_H_

#include <commons/collections/list.h>

typedef struct {
	uint32_t id; // PID
	uint32_t pc; // Program Counter
	uint32_t paginasDeCodigo;
	indiceCodigo* indiceDeCodigo;
	indiceEtiquetas* indiceDeEtiquetas;
	indiceStack* indiceDeStack;
	uint32_t exitCode;
}pcb;

typedef struct{
	uint32_t offsetInicio;
	uint32_t offsetFin;
}indiceCodigo;

//TODO: ver como armar esta estructura
typedef struct{

}indiceEtiquetas;

typedef struct{
	char** args; // Posiciones de memoria de las copias de los argumentos de la función.
	char** vars; // Identificadores y posiciones de memoria de las variables locales.
	uint32_t retPos; // Posicion del indice de codigo donde retorna al finalizar la funcion.
	posicionMemoria* retVar; // Posicion de memoria para guardar el resultado
}indiceStack;

typedef struct{
	uint32_t pag;
	uint32_t offset;
	uint32_t size;
}posicionMemoria;

/* TODO: borrar, ya no se necesita
typedef struct{
	uint32_t id;
	uint32_t pag;
	uint32_t offset;
	uint32_t size;
	uint32_t valor;
}variable;
*/


#endif /* PCB_H_ */
