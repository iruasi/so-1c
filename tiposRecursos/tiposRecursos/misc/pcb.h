#ifndef PCB_H_
#define PCB_H_

#include <stdint.h>
#include <commons/collections/list.h>

typedef struct{
	uint32_t offsetInicio;
	uint32_t offsetFin;
}indiceCodigo;

//TODO: ver como armar esta estructura. quizas se solucione con un char* en pcb
typedef struct{

}indiceEtiquetas;

typedef struct{
	uint32_t pag;
	uint32_t offset;
	uint32_t size;
}posicionMemoria;

typedef struct{
	t_list* args; // Posiciones de memoria de las copias de los argumentos de la función.
	t_list* vars; // Identificadores y posiciones de memoria de las variables locales.
	uint32_t retPos; // Posicion del indice de codigo donde retorna al finalizar la funcion.
	posicionMemoria* retVar; // Posicion de memoria para guardar el resultado
}indiceStack;

typedef struct {
	uint32_t id; // PID
	uint32_t pc; // Program Counter
	uint32_t paginasDeCodigo;
	t_list* indiceDeCodigo;
	char* indiceDeEtiquetas;
	indiceStack* indiceDeStack;
	uint32_t etiquetaSize;
	uint32_t exitCode;
}tPCB;

#endif /* PCB_H_ */
