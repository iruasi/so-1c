#ifndef CONSOLA_H_
#define CONSOLA_H_

#include <commons/config.h>
#include <stdint.h>
#include <stdio.h>

#endif /* CONSOLA_H_ */


typedef struct {

	char *ip_kernel;
	char *puerto_kernel;
	uint32_t tipo_de_proceso;
} tConsola;
typedef struct _t_Package {
	uint32_t tipo_de_proceso;
	uint32_t tipo_de_mensaje;
	uint32_t archivo_size;
	FILE* archivo_a_enviar;
	uint32_t total_size;			// NOTA: Es calculable
} t_Package;

/* Carga los datos de configuracion en una estructura.
 */
tConsola *getConfigConsola(char *ruta_configuracion);

/* Muestra en stdout la configuracion del Kernel
 */
void mostrarConfiguracionConsola(tConsola *consola);

/* Libera todos los recursos allocados de la estructura
 */
void liberarConfiguracionConsola(tConsola *consola);
