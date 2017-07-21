#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>

#include <commons/config.h>
#include <commons/string.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include <commons/bitarray.h>

#include <fuse.h>

#include <funcionesCompartidas/funcionesCompartidas.h>
#include <tiposRecursos/tiposErrores.h>
#include <tiposRecursos/tiposPaquetes.h>

#include "fileSystemConfigurators.h"
#include "operacionesFS.h"
#include "manejadorSadica.h"

#ifndef BACKLOG
#define BACKLOG 20
#endif

enum {
	KEY_VERSION,
	KEY_HELP,
};

static struct fuse_opt fuse_options[] = {
		// Este es un parametro definido por nosotros
		//CUSTOM_FUSE_OPT_KEY("--welcome-msg %s", welcome_msg, 0),

		// Estos son parametros por defecto que ya tiene FUSE
		FUSE_OPT_KEY("-V", KEY_VERSION),
		FUSE_OPT_KEY("--version", KEY_VERSION),
		FUSE_OPT_KEY("-h", KEY_HELP),
		FUSE_OPT_KEY("--help", KEY_HELP),
		FUSE_OPT_END,
};

tFileSystem* fileSystem;
int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	tPackHeader *head_tmp = malloc(sizeof *head_tmp);
	int stat;

	fileSystem = getConfigFS(argv[1]);
	mostrarConfiguracion(fileSystem);

	setupFuseOperations();

	char* argumentos[] = {"", fileSystem->punto_montaje, "f"}; // todo: el primer arg esta hardcodeado, a revisar
	struct fuse_args args = FUSE_ARGS_INIT(3, argumentos);

	// Limpio la estructura que va a contener los parametros
	memset(&runtime_options, 0, sizeof(struct t_runtime_options));

	// Esta funcion de FUSE lee los parametros recibidos y los intepreta
	if (fuse_opt_parse(&args, &runtime_options, fuse_options, NULL) == -1){
		/** error parsing options */
		perror("Invalid arguments!");
		return EXIT_FAILURE;
	}

	//bitmap
	crearDirMetadata(); // las hice separadas porque si la hacia en una se creaba mal la ruta
	crearDirArchivos();
	crearDirBloques();
	crearBitMap();
	meta = getInfoMetadata(rutaBitMap);

	//metadata
	puts("Creando metadata...");
	crearMetadata();
	printf("Ruta donde se crea la carpeta: %s\n", argumentos[1]);
	int ret= fuse_main(args.argc, args.argv, &oper, NULL);

	int sock_lis_kern, sock_kern;
	if ((sock_lis_kern = makeListenSock(fileSystem->puerto_entrada)) < 0){
		printf("No se pudo crear socket listen en puerto: %s", fileSystem->puerto_entrada);
	}

	if((stat = listen(sock_lis_kern, BACKLOG)) == -1){
		perror("Fallo de listen sobre socket Kernel. error");
		return FALLO_GRAL;
	}

	if((sock_kern = makeCommSock(sock_lis_kern)) < 0){
		puts("No se pudo acceptar conexion entrante del Kernel");
		return FALLO_GRAL;
	}

	puts("Esperando handshake de Kernel...");
	while ((stat = recv(sock_kern, head_tmp, sizeof *head_tmp, 0)) > 0){

		printf("Se recibieron %d bytes\n", stat);
		printf("Emisor: %d\nTipo de mensaje: %d", head_tmp->tipo_de_mensaje, head_tmp->tipo_de_mensaje);
	}

	if (stat == -1){
		perror("Error en la realizacion de handshake con Kernel. error");
		return FALLO_RECV;
	}


	printf("Kernel termino la conexion\nLimpiando proceso...\n");
	close(sock_kern);
	liberarConfiguracionFileSystem(fileSystem);
//	return EXIT_SUCCESS;
	return ret; // en todos los ejemplos que vi se retorna el valor del fuse_main..
}

