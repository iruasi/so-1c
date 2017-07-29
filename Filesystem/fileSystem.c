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

int *ker_manejador(void);
int recibirConexionKernel(void);

int sock_kern;
tFileSystem* fileSystem;
t_log *logTrace;
t_list *lista_archivos;

int main(int argc, char* argv[]){

	if(argc!=2){
		printf("Error en la cantidad de parametros\n");
		log_error(logTrace,"Error en la cant de parametros");
		return EXIT_FAILURE;
	}

	lista_archivos = list_create();
	logTrace = log_create("/home/utnso/logFILESYSTEMTrace.txt","FILESYSTEM",0,LOG_LEVEL_TRACE);
	log_trace(logTrace,"\n\n\n\n\n Inicia nueva ejecucion de FILESYSTEM \n\n\n\n\n");

	int stat, *retval;
	pthread_t kern_th;

	fileSystem = getConfigFS(argv[1]);
	mostrarConfiguracion(fileSystem);

	crearDirMontaje();
	crearDirectoriosBase();
	if (inicializarMetadata() != 0){
		log_error(logTrace,"no se pudo levantar el metadata del filesystem");
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
	}

	pthread_create(&kern_th, NULL, (void *) ker_manejador, &retval);
	pthread_join(kern_th, (void **) &retval);

	close(sock_kern);
	liberarConfiguracionFileSystem(fileSystem);
	return 0;
}


int *ker_manejador(void){
	log_trace(logTrace,"inicio thread ker manejador");
	int stat, pack_size, bytes_leidos;
	int *retval = malloc(sizeof(int));
	tPackHeader head = {.tipo_de_proceso = KER, .tipo_de_mensaje = 9852};//9852, iniciar escuchaKernel
	char *buffer, *info, *abs_path;
	tPackBytes * abrir, *bytes, *borrar;
	tPackRecvRW *rw;
	tPackRecibirRW *io;

	do {
	switch(head.tipo_de_mensaje){

	case VALIDAR_ARCHIVO:
		log_trace(logTrace,"Se pide validacion de archivo");

		buffer = recvGeneric(sock_kern);
		abrir = deserializeBytes(buffer);
		freeAndNULL((void **) &buffer);

		abs_path = hacerPathAbsolutoArchivos(abrir->bytes);

		head.tipo_de_proceso = FS;
		head.tipo_de_mensaje = (validarArchivo(abs_path) == -1)?
				INVALIDAR_RESPUESTA: VALIDAR_RESPUESTA;
		informarResultado(sock_kern, head);

		free(abrir->bytes);
		freeAndNULL((void **) &abs_path);
		freeAndNULL((void **) &abrir);
		break;

	case CREAR_ARCHIVO:
		log_trace(logTrace,"Se pide crear archivo");

		buffer = recvGeneric(sock_kern);
		abrir = deserializeBytes(buffer);
		freeAndNULL((void **) &buffer);

		abs_path = hacerPathAbsolutoArchivos(abrir->bytes);
		free(abrir->bytes);
		freeAndNULL((void **) &abrir);

		head.tipo_de_proceso = FS;
		head.tipo_de_mensaje = (crearArchivo(abs_path) < 0)?
				INVALIDAR_RESPUESTA : CREAR_ARCHIVO;
		informarResultado(sock_kern, head);

		freeAndNULL((void **) &abs_path);
		break;

	case BORRAR:
		log_trace(logTrace, "Se peticiona borrar un archivo");

		buffer = recvGeneric(sock_kern);
		bytes  = deserializeBytes(buffer);
		free(bytes->bytes);
		freeAndNULL((void **) &bytes);

		abs_path = hacerPathAbsolutoArchivos(bytes->bytes);

		head.tipo_de_mensaje = (unlink2(abs_path) != 0)?
				INVALIDAR_RESPUESTA : ARCHIVO_BORRADO;
		informarResultado(sock_kern,head);

		freeAndNULL((void **) &buffer);
		break;

	case LEER:
		log_trace(logTrace,"se peticiona la lectura de un archivo");

		buffer = recvGeneric(sock_kern);
		rw = deserializeLeerFS2(buffer);

		abs_path = hacerPathAbsolutoArchivos(rw->direccion);

		if((info = read2(abs_path, rw->size, rw->cursor, &bytes_leidos)) == NULL){
			puts("Hubo un fallo ejecutando read2");
			log_error(logTrace,"Fallo la lectura de %d bytes del file %s", rw->size, abs_path);
			head.tipo_de_mensaje = INVALIDAR_RESPUESTA;
			informarResultado(sock_kern, head);
			break;
		}

		log_trace(logTrace,"el archivo fue leido con exito");
		head.tipo_de_proceso = FS; head.tipo_de_mensaje = ARCHIVO_LEIDO;
		buffer = serializeBytes(head, info, rw->size, &pack_size);

		if ((stat = send(sock_kern, buffer, pack_size, 0)) == -1){
			perror("Fallo send de bytes leidos a Kernel. error");
			log_trace(logTrace,"fallo send de bytes leiudos a kernel");
		}
		log_trace(logTrace,"se enviaron %d bytes a kernel",stat);

		freeAndNULL((void **) &abs_path);
		freeAndNULL((void **) &buffer);
		free(rw->direccion); freeAndNULL((void **) &rw);
	//	free(bytes->bytes);  freeAndNULL((void **) &bytes);
		break;

	case ESCRIBIR:
		log_trace(logTrace, "Se escribe un archivo");

		buffer = recvGeneric(sock_kern);
		io = deserializeLeerFS(buffer);

		abs_path = hacerPathAbsolutoArchivos(io->direccion);

		head.tipo_de_proceso = FS;
		head.tipo_de_mensaje = (write2(abs_path, io->info, io->tamanio, io->cursor) < 0)?
			FALLO_ESCRITURA : ARCHIVO_ESCRITO;
		informarResultado(sock_kern, head);

		freeAndNULL((void **) &abs_path);
		freeAndNULL((void **) &io);
		freeAndNULL((void **) &buffer);
		break;

	default:
		//puts("Se recibio un mensaje no manejado!");
		log_info(logTrace,"se recibio un msj no manejado; PROC %d MENSAJE %d", head.tipo_de_proceso, head.tipo_de_mensaje);
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
