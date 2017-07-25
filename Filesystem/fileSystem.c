#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <pthread.h>

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

int *ker_manejador(void);
int recibirConexionKernel(void);

int sock_kern;
extern char *rutaBitMap;
extern tMetadata* meta;
tFileSystem* fileSystem;

int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		return EXIT_FAILURE;
	}

	int stat, fuseret, *retval;
	pthread_t kern_th;

	fileSystem = getConfigFS(argv[1]);
	mostrarConfiguracion(fileSystem);

	setupFuseOperations();

	char* argumentos[] = {"", fileSystem->punto_montaje, ""};
	struct fuse_args args = FUSE_ARGS_INIT(3, argumentos);

	// Limpio la estructura que va a contener los parametros
	memset(&runtime_options, 0, sizeof(struct t_runtime_options));

	// Esta funcion de FUSE lee los parametros recibidos y los intepreta
	if (fuse_opt_parse(&args, &runtime_options, fuse_options, NULL) == -1){
		perror("Invalid arguments!");
		return EXIT_FAILURE;
	}

	//bitmap
	crearDirMontaje();
	if ((fuseret = fuse_main(args.argc, args.argv, &oper, NULL)) == -1){
		perror("Error al operar fuse_main. error");
		return FALLO_GRAL;
	}

	crearDirectoriosBase();

	//metadata
	puts("Creando metadata...");
	crearMetadata();
	printf("Ruta donde se crea la carpeta: %s\n", argumentos[1]);
	meta = getInfoMetadata();

	if ((stat = recibirConexionKernel()) < 0){
		puts("No se pudo conectar con Kernel!");
		//todo: limpiarFilesystem();
	}

	pthread_create(&kern_th, NULL, (void *) ker_manejador, &retval);

	pthread_join(kern_th, NULL);

	close(sock_kern);
	liberarConfiguracionFileSystem(fileSystem);
//	return EXIT_SUCCESS;
	return fuseret; // en todos los ejemplos que vi se retorna el valor del fuse_main..
}


int *ker_manejador(void){

	int stat;
	int *retval = malloc(sizeof(int));
	tPackHeader head;

	do {
	switch(head.tipo_de_mensaje){

	case VALIDAR_ARCHIVO:
		puts("Se pide validacion de archivo");
		puts("Fin case VALIDAR_ARCHIVO");
		break;

	case CREAR_ARCHIVO:
		puts("Se pide crear un archivo");
		puts("Fin case CREAR_ARCHIVO");
		break;

	case ARCHIVO_BORRADO:// todo: interprete bien lo de borrado? o es el mensaje de respuesta? Esta definido BORRAR, en \todo caso..
		puts("Se peticiona el borrado de un archivo");
		puts("Fin case ARCHIVO_BORRADO");
		break;

	case ARCHIVO_LEIDO:
		puts("Se peticiona la lectura de un archivo");
		puts("Fin case ARCHIVO_LEIDO");
		break;

	case ARCHIVO_ESCRITO:
		puts("Se peticiona la escritura de un archivo");
		puts("Fin case ARCHIVO_ESCRITO");
		break;

	default:
		puts("Se recibio un mensaje no manejado!");
		printf("Proc %d, Mensaje %d\n", head.tipo_de_proceso, head.tipo_de_mensaje);
		break;

	}} while((stat = recv(sock_kern, &head, HEAD_SIZE, 0)) > 0);

	if (stat == -1){
		perror("Fallo recepcion de Kernel. error");
		*retval = FALLO_RECV;
		return retval;
	}

	puts("Kernel cerro la conexion");
	*retval = 0;
	return retval;
}

int recibirConexionKernel(void){

	int sock_lis_kern;
	tPackHeader head, h_esp;
	if ((sock_lis_kern = makeListenSock(fileSystem->puerto_entrada)) < 0){
		printf("No se pudo crear socket listen en puerto: %s", fileSystem->puerto_entrada);
		return FALLO_GRAL;
	}

	if(listen(sock_lis_kern, BACKLOG) == -1){
		perror("Fallo de listen sobre socket Kernel. error");
		return FALLO_GRAL;
	}

	h_esp.tipo_de_proceso = KER; h_esp.tipo_de_mensaje = HSHAKE;
	while (1){
		if((sock_kern = makeCommSock(sock_lis_kern)) < 0){
			puts("No se pudo acceptar conexion entrante del Kernel");
			return FALLO_GRAL;
		}

		if (validarRespuesta(sock_kern, h_esp, &head) != 0){
			printf("Rechazo proc %d mensaje %d\n", h_esp.tipo_de_proceso, h_esp.tipo_de_mensaje);
			close(sock_kern);
			continue;
		}
		printf("Se establecio conexion con Kernel. Socket %d\n", sock_kern);
		break;
	}

	return 0;
}
