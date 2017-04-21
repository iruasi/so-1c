#ifndef CONSOLA_H_
#define CONSOLA_H_

#include <commons/config.h>

#endif /* CONSOLA_H_ */


typedef struct {

	char *ip_kernel;
	char *puerto_kernel;
} tConsola;

/* Carga los datos de configuracion en una estructura.
 * Si alguno de los datos de config no se encontraron, retorna NULL;
 */
tConsola *getConfigConsola(char *ruta_configuracion);

/* Muestra en stdout la configuracion del Kernel
 */
void mostrarConfiguracionConsola(tConsola *consola);

/* Libera todos los recursos allocados de la estructura
 */
void liberarConfiguracionConsola(tConsola *consola);
