#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

#include <commons/config.h>
#include <commons/string.h>
#include <commons/log.h>
#include <commons/collections/list.h>

#include <fuse.h>

#include <funcionesCompartidas/funcionesCompartidas.h>
#include <tiposRecursos/tiposErrores.h>
#include <tiposRecursos/tiposPaquetes.h>

#include "fileSystemConfigurators.h"
#include "operacionesFS.h"

#ifndef BACKLOG
#define BACKLOG 20
#endif

#define bitMap "/Metadata/Bitmap.bin"
#define metadata "/Metadata/Metadata.bin"

void crearArchivo(char* ruta); //todo: sacar a auxiliares
void crearBloques(void);
void crearMetadata(void);
void crearDirMetadata(void);
void crearDirArchivos(void);
void crearDirBloques(void);

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

	char* argumentos[] = {"", fileSystem->punto_montaje, "f"};
	struct fuse_args args = FUSE_ARGS_INIT(3, argumentos);

	// Limpio la estructura que va a contener los parametros
	memset(&runtime_options, 0, sizeof(struct t_runtime_options));

	// Esta funcion de FUSE lee los parametros recibidos y los intepreta
	if (fuse_opt_parse(&args, &runtime_options, fuse_options, NULL) == -1){
		/** error parsing options */
		perror("Invalid arguments!");
		return EXIT_FAILURE;
	}

	// Esta es la funcion principal de FUSE, es la que se encarga
	// de realizar el montaje, comuniscarse con el kernel, delegar todo
	// en varios threads

	//bitmap
	crearDirMetadata();
	crearDirArchivos();
	crearDirBloques();
	puts("Creando bitmap...");
	char* rutaBitMap = malloc(sizeof(fileSystem->punto_montaje) + sizeof(bitMap));
	memcpy(rutaBitMap, fileSystem->punto_montaje, sizeof(fileSystem->punto_montaje));
	memcpy(rutaBitMap+sizeof(fileSystem->punto_montaje), bitMap, sizeof(bitMap));
	FILE* bitmap = fopen(rutaBitMap, "wb");
	printf("Ruta del bitmap: %s\n", rutaBitMap);
	free(rutaBitMap);
	puts("Creando metadata...");
	printf("Ruta donde se crea la carpeta: %s\n", argumentos[0]);
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
	return ret;
}

void crearArchivo(char* ruta){
	int off=0;
	char* rutaArchivo = malloc(sizeof(fileSystem->punto_montaje) + sizeof("/Archivos/") + sizeof(ruta));
	memcpy(rutaArchivo, fileSystem->punto_montaje, sizeof(fileSystem->punto_montaje));
	off+=sizeof(fileSystem->punto_montaje);
	memcpy(rutaArchivo+off, "/Archivos/", sizeof("/Archivos/"));
	off+=sizeof(ruta);
	memcpy(rutaArchivo+off, ruta, sizeof(ruta));
	FILE* archivo = fopen(rutaArchivo, "w+");
	printf("Ruta del archivo: %s", rutaArchivo);
	free(rutaArchivo);
}

void crearBloques(){
	puts("Creando bloques..");
	int off=0;
	int i;
	for(i=0; i<=5191; i++){ //todo: la cantidad se va a sacar del bitmap
		char* nro = malloc(10); //no va a superar esta cantidad de numeros..
		sprintf(nro, i);
		char* rutaBloque = malloc(sizeof(fileSystem->punto_montaje) + sizeof("/Bloques/") + sizeof(nro) + sizeof(".bin"));
		memcpy(rutaBloque, fileSystem->punto_montaje, sizeof(fileSystem->punto_montaje));
		off+=sizeof(fileSystem->punto_montaje);
		memcpy(rutaBloque+off, "/Bloques/", sizeof("/Bloques/"));
		off+=sizeof(nro);
		memcpy(rutaBloque+off, nro, sizeof(nro));
		off+=sizeof(".bin");
		memcpy(rutaBloque+off, ".bin", sizeof(".bin"));
		FILE* bloque = fopen(rutaBloque, "w+");
		free(rutaBloque);
		free(nro);
	}
}

void crearMetadata(){
	char* rutaMetadata = malloc(sizeof(fileSystem->punto_montaje) + sizeof(metadata));
	memcpy(rutaMetadata, fileSystem->punto_montaje, sizeof(fileSystem->punto_montaje));
	memcpy(rutaMetadata+sizeof(fileSystem->punto_montaje), metadata, sizeof(metadata));
	printf("Ruta de la metadata: %s\n"
			"", rutaMetadata);
	FILE* metadataArch = fopen(rutaMetadata, "wb");
	free(rutaMetadata);
}

void crearDirMetadata(){
	char* rutaMetadata = malloc(sizeof(fileSystem->punto_montaje) + sizeof("/Metadata"));
	int off=0;
	memcpy(rutaMetadata, fileSystem->punto_montaje, sizeof(fileSystem->punto_montaje));
	off+=sizeof(fileSystem->punto_montaje);
	memcpy(rutaMetadata+off, "/Metadata", sizeof("/Metadata"));
	mkdir(rutaMetadata, S_IWUSR); //para que el usuario pueda escribir
	printf("Directorio %s creado!\n", rutaMetadata);
	free(rutaMetadata);
}

void crearDirArchivos(){
	char* rutaArchivos = malloc(sizeof(fileSystem->punto_montaje) + sizeof("/Archivos"));
	int off=0;
	memcpy(rutaArchivos, fileSystem->punto_montaje, sizeof(fileSystem->punto_montaje));
	off+=sizeof(fileSystem->punto_montaje);
	memcpy(rutaArchivos+off, "/Archivos", sizeof("/Archivos"));
	mkdir(rutaArchivos, S_IWUSR); //para que el usuario pueda escribir
	printf("Directorio %s creado!\n", rutaArchivos);
	free(rutaArchivos);
}
void crearDirBloques(){
	char* rutaBloques = malloc(sizeof(fileSystem->punto_montaje) + sizeof("/Bloques"));
	int off=0;
	memcpy(rutaBloques, fileSystem->punto_montaje, sizeof(fileSystem->punto_montaje));
	off+=sizeof(fileSystem->punto_montaje);
	memcpy(rutaBloques+off, "/Bloques", sizeof("/Bloques"));
	mkdir(rutaBloques, S_IWUSR); //para que el usuario pueda escribir
	printf("Directorio %s creado!\n", rutaBloques);
	free(rutaBloques);
}
