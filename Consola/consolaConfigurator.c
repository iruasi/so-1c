#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "consolaConfigurator.h"

#define MAX_IP_LEN 16   // aaa.bbb.ccc.ddd -> son 15 caracteres, 16 contando un '\0'
#define MAX_PORT_LEN 6  // 65535 -> 5 digitos, 6 contando un '\0'

/* Carga los datos de configuracion en una estructura.
 * Si alguno de los datos de config no se encontraron, retorna NULL;
 */
tConsola *getConfigConsola(char *ruta){

	// Alojamos espacio en memoria para la esctructura que almacena los datos de config
	tConsola *consola      = malloc(sizeof *consola);
	consola->ip_kernel     = malloc(MAX_IP_LEN);
	consola->puerto_kernel = malloc(MAX_PORT_LEN);

	t_config *consola_conf = config_create(ruta);

	if (!config_has_property(consola_conf, "IP_KERNEL") || !config_has_property(consola_conf, "PUERTO_KERNEL")){
		printf("No se detecto alguno de los parametros de configuracion!");
		return NULL;
	}

	strcpy(consola->ip_kernel,     config_get_string_value(consola_conf, "IP_KERNEL"));
	strcpy(consola->puerto_kernel, config_get_string_value(consola_conf, "PUERTO_KERNEL"));

	config_destroy(consola_conf);
	return consola;
}

void mostrarConfiguracionConsola(tConsola *cons_data){

	printf("%s\n", cons_data->ip_kernel);
	printf("%s\n", cons_data->puerto_kernel);
}

void liberarConfiguracionConsola(tConsola *cons){

	free(cons->ip_kernel);
	free(cons->puerto_kernel);
	free(cons);
}
