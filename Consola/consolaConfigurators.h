#ifndef CONSOLA_H_
#define CONSOLA_H_

#include <commons/config.h>
#include <stdint.h>

#endif /* CONSOLA_H_ */


typedef struct {

	char *ip_kernel;
	char *puerto_kernel;
	uint32_t tipo_de_proceso;
} tConsola;

/* Carga los datos de configuracion en una estructura.
 */
tConsola *getConfigConsola(char *ruta_configuracion);

/* Muestra en stdout la configuracion del Kernel
 */
void mostrarConfiguracionConsola(tConsola *consola);

/* Libera todos los recursos allocados de la estructura
 */
void liberarConfiguracionConsola(tConsola *consola);
