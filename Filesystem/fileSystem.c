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

#include <funcionesPaquetes/funcionesPaquetes.h>
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
t_log *logTrace;


int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		log_error(logTrace,"Error en nla cant de parametros");
		return EXIT_FAILURE;
	}


	logTrace = log_create("/home/utnso/logFILESYSTEMTrace.txt","FILESYSTEM",0,LOG_LEVEL_TRACE);

		log_trace(logTrace,"");
		log_trace(logTrace,"");
		log_trace(logTrace,"");
		log_trace(logTrace,"");
		log_trace(logTrace,"");
		log_trace(logTrace,"");
		log_trace(logTrace,"");
		log_trace(logTrace,"Inicia nueva ejecucion de FILESYSTEM");
		log_trace(logTrace,"");
		log_trace(logTrace,"");
		log_trace(logTrace,"");
		log_trace(logTrace,"");
		log_trace(logTrace,"");
		log_trace(logTrace,"");




	int stat, fuseret, *retval;
	pthread_t kern_th;

	fileSystem = getConfigFS(argv[1]);
	mostrarConfiguracion(fileSystem);

	setupFuseOperations();

	char* argumentos[] = {"", fileSystem->punto_montaje, ""};
	struct fuse_args args = FUSE_ARGS_INIT(3, argumentos);

	// Limpio la estructura que va a contener los parametros
	memset(&runtime_options, 0, sizeof(struct t_runtime_options));
	log_trace(logTrace,"limpio la estructura q va a contener a los parametrosd");
	// Esta funcion de FUSE lee los parametros recibidos y los intepreta
	if (fuse_opt_parse(&args, &runtime_options, fuse_options, NULL) == -1){
		perror("Invalid arguments!");
		log_error(logTrace,"invalid arguments");
		return EXIT_FAILURE;
	}

	//bitmap
	crearDirMontaje();
	if ((fuseret = fuse_main(args.argc, args.argv, &oper, NULL)) == -1){
		perror("Error al operar fuse_main. error");
		log_error(logTrace,"error al operar fuse_main");
		return FALLO_GRAL;
	}

	crearDirectoriosBase();
	if (inicializarMetadata() != 0){
		log_error(logTrace,"no s epudo levantaar el metadata del filesystem");
		puts("No se pudo levantar el Metadata del Filesystem!");
		return ABORTO_FILESYSTEM;
	}
	if (inicializarBitmap() != 0){
		log_error(logTrace,"no se pudo levantar el bitmap del filesystem");
		puts("No se pudo levantar el Bitmap del Filesystem!");
		return ABORTO_FILESYSTEM;
	}

	if ((stat = recibirConexionKernel()) < 0){
		log_error(logTrace,"no se pudo conectar con kernel");
		puts("No se pudo conectar con Kernel!");
		//todo: limpiarFilesystem();
	}

	// todo: en vez del ker, habria que combinarlo con el manejador del manejadorSadica.c
	pthread_create(&kern_th, NULL, (void *) ker_manejador, &retval);
	pthread_join(kern_th, (void **) &retval);

	close(sock_kern);
	liberarConfiguracionFileSystem(fileSystem);
	return fuseret; // en todos los ejemplos que vi se retorna el valor del fuse_main..
}


int *ker_manejador(void){
	log_trace(logTrace,"inicio thread ker manejador");
	int stat, pack_size;
	int *retval = malloc(sizeof(int));
	tPackHeader head = {.tipo_de_proceso = KER, .tipo_de_mensaje = 9852};//9852, iniciar escuchaKernel
	tPackHeader header;
	char *buffer, *info;
	tPackBytes * abrir, *bytes;
	tPackBytes * borrar;
	tPackRecvRW *rw;
	tPackRecibirRW * leer;
	tPackRecibirRW * escribir;
	struct fuse_file_info *fi = malloc(sizeof *fi);
	int operacion;
	header.tipo_de_proceso = FS;

	do {
	switch(head.tipo_de_mensaje){

	case VALIDAR_ARCHIVO:
		log_trace(logTrace,"Se pide validacion de archivo");
		//puts("Se pide validacion de archivo");
		buffer = recvGeneric(sock_kern);
		abrir = deserializeBytes(buffer);

		head.tipo_de_proceso = FS;
		head.tipo_de_mensaje = (validarArchivo(abrir->bytes) == -1)?
				INVALIDAR_RESPUESTA: VALIDAR_RESPUESTA;
		informarResultado(sock_kern, head);

		freeAndNULL((void **)&buffer);
		freeAndNULL((void **)&abrir);
		//puts("Fin case VALIDAR_ARCHIVO");
		break;

	case CREAR_ARCHIVO:
		//puts("Se pide crear un archivo");
		log_trace(logTrace,"se pide crear archivo");
		buffer = recvGeneric(sock_kern);
		abrir = deserializeBytes(buffer);

		head.tipo_de_proceso = FS;
		head.tipo_de_mensaje = (crearArchivo(abrir->bytes) < 0)?
				INVALIDAR_RESPUESTA : CREAR_ARCHIVO;
		informarResultado(sock_kern,header);

		//puts("Fin case CREAR_ARCHIVO");
		break;

	case BORRAR:
		//puts("Se peticiona el borrado de un archivo");
		log_trace(logTrace,"se peticiona el borrado de un archivo");
		buffer = recvGeneric(sock_kern);
		borrar = deserializeBytes(buffer);
		if((operacion = unlink2(borrar->bytes)) == 0){
			//puts("El archivo fue borrado con exito");
			log_trace(logTrace,"el archivo fue borrado con exito");
			header.tipo_de_mensaje = ARCHIVO_BORRADO;
			informarResultado(sock_kern,header);
		}else{
			log_error(logTrace,"el archivo no pudo ser borrado");
			puts("El archivo no pudo ser borrado");
			header.tipo_de_mensaje = INVALIDAR_RESPUESTA;
			informarResultado(sock_kern,header);
		}
		//puts("Fin case BORRAR");
		freeAndNULL((void **)&buffer);
		break;

	case LEER:
		//puts("Se peticiona la lectura de un archivo");
		log_trace(logTrace,"se peticiona la lectura de un archivo");
		buffer = recvGeneric(sock_kern);
		rw = deserializeLeerFS2(buffer);

		info = malloc(rw->size);

		if(read2(rw->direccion, &info, rw->size, rw->cursor, fi) != 0){
			puts("Hubo un fallo ejecutando read2");

			log_error(logTrace,"hubo un fallo ejecutando read2");
			head.tipo_de_mensaje = INVALIDAR_RESPUESTA;
			informarResultado(sock_kern, head);
		}
		//puts("El archivo fue leido con exito");
		log_trace(logTrace,"el archivo fue leido con exito");
		head.tipo_de_proceso = FS; head.tipo_de_mensaje = ARCHIVO_LEIDO;
		bytes = serializeBytes(head, info, rw->size, &pack_size);

		if ((stat = send(sock_kern, bytes, pack_size, 0)) == -1){
			perror("Fallo send de bytes leidos a Kernel. error");
			log_trace(logTrace,"fallo send de bytes leiudos a kernel");
			break;
		}
		//printf("Se enviaron %d bytes a Kernel\n", stat);
		log_trace(logTrace,"se enviaron %d bytes a kernel",stat);
		//puts("Fin case LEER");

		freeAndNULL((void **) &buffer);
		free(rw->direccion); freeAndNULL((void **) &rw);
		free(bytes->bytes);  freeAndNULL((void **) &bytes);
		break;

	case ESCRIBIR:
		//puts("Se peticiona la escritura de un archivo");
		log_trace(logTrace,"se peticiona la escritura de un archivo");
		buffer = recvGeneric(sock_kern);
		escribir = deserializeLeerFS(buffer);
		if(true){//(operacion = write2(escribir->direccion)) == 0
			//puts("El archivo fue escrito con exito");
			log_trace(logTrace,"el archivo fue escrito con exito");
			header.tipo_de_mensaje = ARCHIVO_ESCRITO;
			informarResultado(sock_kern,header);
		}else{
			log_error(logTrace,"el archivo no pudo ser borrado");
			puts("El archivo no pudo ser borrado");
			header.tipo_de_mensaje = INVALIDAR_RESPUESTA;
			informarResultado(sock_kern,header);
		}
		//puts("Fin case ESCRIBIR");
		freeAndNULL((void **)&buffer);
		break;

	default:
		//puts("Se recibio un mensaje no manejado!");
		log_info(logTrace,"se recibio un msj no manejado; PROC %d MENSAJE %d",head.tipo_de_proceso,head_tipo_de_mensaje);
		//printf("Proc %d, Mensaje %d\n", head.tipo_de_proceso, head.tipo_de_mensaje);
		break;

	}} while((stat = recv(sock_kern, &head, HEAD_SIZE, 0)) > 0);

	if (stat == -1){
		log_error(logTrace,"fallo recepcion de kernel");
		perror("Fallo recepcion de Kernel. error");
		*retval = FALLO_RECV;
		return retval;
	}
	log_trace(logTrace,"kern el cerro la conexion");
	puts("Kernel cerro la conexion");
	*retval = FIN;
	return retval;
}

int recibirConexionKernel(void){

	int sock_lis_kern;
	tPackHeader head, h_esp;
	if ((sock_lis_kern = makeListenSock(fileSystem->puerto_entrada)) < 0){
		printf("No se pudo crear socket listen en puerto: %s\n", fileSystem->puerto_entrada);
		log_error(logTrace,"no se pudo crear socket listen en puerto %s",fileSystem->puerto_entrada);
		return FALLO_GRAL;
	}

	if(listen(sock_lis_kern, BACKLOG) == -1){
		log_error(logTrace,"fallo de listen sobre socket kernel");
		perror("Fallo de listen sobre socket Kernel. error");
		return FALLO_GRAL;
	}

	h_esp.tipo_de_proceso = KER; h_esp.tipo_de_mensaje = HSHAKE;
	while (1){
		if((sock_kern = makeCommSock(sock_lis_kern)) < 0){
			log_error(logTrace,"no s epudo aceptar conexion entrante del kernel");
			puts("No se pudo acceptar conexion entrante del Kernel");
			return FALLO_GRAL;
		}

		if (validarRespuesta(sock_kern, h_esp, &head) != 0){
			log_trace(logTrace,"rechazo proc %d por mensaje %d",h_esp.tipo_de_proceso,h_esp.tipo_de_mensaje);
			//printf("Rechazo proc %d mensaje %d\n", h_esp.tipo_de_proceso, h_esp.tipo_de_mensaje);
			close(sock_kern);
			continue;
		}
		log_trace(logTrace,"se establecio conexion c kernel socket %d",sock_kern);
		printf("Se establecio conexion con Kernel. Socket %d\n", sock_kern);
		break;
	}

	return 0;
}
