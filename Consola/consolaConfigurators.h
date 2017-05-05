#ifndef CONSOLA_H_
#define CONSOLA_H_

#include <commons/config.h>
#include <stdint.h>
#include <stdio.h>
#define MAX_MESSAGE_SIZE 5000

#endif /* CONSOLA_H_ */


typedef struct {

	char *ip_kernel;
	char *puerto_kernel;
	uint32_t tipo_de_proceso;
} tConsola;
typedef struct _t_PackageEnvio {
	uint32_t tipo_de_proceso;
	uint32_t tipo_de_mensaje;
	uint32_t message_size;
	char* message;
	uint32_t total_size;			// NOTA: Es calculable
} t_PackageEnvio;

typedef struct _t_PackageRecepcion {
	uint32_t tipo_de_proceso;
	uint32_t tipo_de_mensaje;
	char message[MAX_MESSAGE_SIZE];
	uint32_t message_size;			// NOTA: Es calculable. Aca lo tenemos por fines didacticos!
} t_PackageRecepcion;

char* serializarOperandos(t_PackageEnvio*);
int recieve_and_deserialize(t_PackageRecepcion *package, int socketCliente);



/* Carga los datos de configuracion en una estructura.
 */
tConsola *getConfigConsola(char *ruta_configuracion);

/* Muestra en stdout la configuracion del Kernel
 */
void mostrarConfiguracionConsola(tConsola *consola);

/* Libera todos los recursos allocados de la estructura
 */
void liberarConfiguracionConsola(tConsola *consola);
