#include <stddef.h>
#include <stdlib.h>
//#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>

#include <commons/log.h>
#include <commons/collections/list.h>
#include <commons/config.h>

#include "operacionesFS.h"
#include "manejadorSadica.h"

extern t_bitarray *bitArray;
extern tMetadata *meta;

extern t_log *logTrace;

//static int getattr(const char *path, struct stat *stbuf) {
//	int res = 0;
//	log_trace(logTrace,"funcion getattr");
//	memset(stbuf, 0, sizeof(struct stat));
//
//	//Si path es igual a "/" nos estan pidiendo los atributos del punto de montaje
//
//	if (strcmp(path, "/") == 0) {
//		stbuf->st_mode = S_IFDIR | 0755;
//		stbuf->st_nlink = 2;
//	} else if (strcmp(path, DEFAULT_FILE_PATH) == 0) {
//		stbuf->st_mode = S_IFREG | 0444;
//		stbuf->st_nlink = 1;
//		stbuf->st_size = strlen(DEFAULT_FILE_CONTENT);
//	} else {
//		res = -ENOENT;
//	}
//	return res;
//}
//
//static int readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
//	(void) offset;
//	log_trace(logTrace,"funcion readdir");
//	(void) fi;
//
//	if (strcmp(path, "/") != 0)
//		return -ENOENT;
//
//	// "." y ".." son entradas validas, la primera es una referencia al directorio donde estamos parados
//	// y la segunda indica el directorio padre
//	filler(buf, ".", NULL, 0);
//	filler(buf, "..", NULL, 0);
//	filler(buf, DEFAULT_FILE_NAME, NULL, 0);
//	//printf("Leyendo los archivos de %s\n", path);
//	log_trace(logTrace,"leyendo los archivos de %s ",path);
//	return 0;
//}

int open2(char *path) {
	int bloque=-1;
	int i=0;
	log_trace(logTrace,"funcion open2");
	tArchivos* arch = malloc(sizeof(tArchivos));
	/*
	if (strcmp(path, DEFAULT_FILE_PATH) != 0)
		return -ENOENT;

	if ((fi->flags & 3) != O_RDONLY)
		return -EACCES;
	*/
	//puts ("Se quiere abrir un archivo");
	log_trace(logTrace,"se quiere abrir un archivo");

	while(i<meta->cantidad_bloques && bloque==-1){
		if(!bitarray_test_bit(bitArray, i)){
			bitarray_set_bit(bitArray, i);
			bloque=i;
		}
	}
	if(bloque >= 0){
		fopen(path, "w"); //todo: ver como manejar las banderas
		//todo: el modo creacion seria ese?
		sprintf(arch->bloques[0], bloque);
		memcpy(arch->ruta, path, sizeof(path));
		arch->fd = fileno(path);
		list_add(lista_archivos, arch);
		log_trace(logTrace,"se creo el archivo");
		//puts("Se creo el archivo!\n");
	}
	else{
		log_error(logTrace,"no hay espacio para crear el archivo");
		perror("No hay espacio para crear el archivo..");
		free(arch);
		return -1;
	}


	return fileno(path);
}

int read2(char *path, char **buf, size_t size, off_t offset) {
	/*size_t len;
	(void) fi;
	if (strcmp(path, DEFAULT_FILE_PATH) != 0)
		return -ENOENT;
	len = strlen(DEFAULT_FILE_CONTENT);
	if (offset < len) {
		if (offset + size > len)
			size = len - offset;
		memcpy(buf, DEFAULT_FILE_CONTENT + offset, size);
	} else
		size = 0;
		*/
	log_trace(logTrace,"se quiere leer el archivo %s size:%d offset:%d",path,size,offset);
	//printf("Se quiere leer el archivo %s, size: %d, offset: %d\n", path, size, offset);
	if(validarArchivo(path)==-1){
		log_error(logTrace,"error al leer el archivo");
		perror("Error al leer el archivo...");
		return -1;
	}
	fread(*buf, size, 1, path);
	//printf("Datos leidos: %s\n", *buf);
	log_trace(logTrace,"datos leidos %s",buf);
	return size;
}

int write2(char * path, char * buf, size_t size, off_t offset){
	int cantidadBloques = (int) ceil((float) size/meta->tamanio_bloques);
	int bloquesLibres=0,
			i=0;
	t_list* bloques;
	log_trace(logTrace,"se quiere escribir en %s los datos de %s el tamanio %d",path,buf,size);
	//printf("Se quiere escribir en %s los datos de %s el tamanio %d", path, buf, size);
	if(validarArchivo(path)==-1){
		log_error(logTrace,"error al escribir archivo");
		perror("Error al escribir archivo");
		return -1;
	}
	while(bloquesLibres<cantidadBloques && i<meta->cantidad_bloques){
		if(!bitarray_test_bit(bitArray, i)){
			list_add(bloques, i);
			++bloquesLibres;
		}
	}
	if(bloquesLibres<cantidadBloques){
		log_error(logTrace,"no hay bloques suficientes para escribir los datos");
		perror("No hay bloques suficientes para escribir los datos");
		return -1; // en caso de error, retorna -1 y finaliza.
	}
	while(!list_is_empty(bloques)){
		bitarray_set_bit(bitArray, list_get(bloques, 0));
		list_remove(bloques, 0);
	}
	fwrite(buf, size, offset, (void*)path);
	//puts("Se escribieron los datos en el archivo!\n");
	log_trace(logTrace,"se escribieron los datos en el archivo");
	list_destroy(bloques);
	return size;
}

int unlink2 (char *path){
	//int rem;
	log_trace(logTrace,"se quiere borrar el archivo %s",path);
	//printf("Se quiere borrar el archivo el archivo %s\n", path);
	if(validarArchivo(path)==-1){
		log_error(logTrace,"error al borrar archivo");
		perror("Error al borrar archivo");
		return -1;
	}
	tArchivos* archivo = malloc(sizeof(tArchivos));
	t_config* conf = config_create(path);
	archivo->bloques = config_get_array_value(conf, "BLOQUES");
	int i;
	for(i=0; i<sizeof(archivo->bloques)/sizeof(archivo->bloques[0]); i++){
		bitarray_clean_bit(bitArray, atoi(archivo->bloques[i]));
	}
	for(i=0; i<list_size(lista_archivos); i++){
		archivo = list_get(lista_archivos, i);
		if(archivo->fd == fileno(path)){
			list_remove(lista_archivos, i);
			break;
		}
	}
	remove(path);
	free(archivo);
	config_destroy(conf);
	return 0;
}

/*
 * Validar no se bien que funcion fuse es, pero la pide el enunciado,
 * quizas sea "interna" previa a leer o escribir datos.
 */
int validarArchivo(char* path){
	//printf("Se quiere validar la existencia del archivo %s\n", path);
	log_trace(logTrace,"se quiere validar la existencia del archivo %s",path);
	FILE* arch;
	if((arch = fopen(path, "rb")) == NULL){
		log_error(logTrace,"el archivo no existe");
		perror("El archivo no existe...\n");
		return -1;
	}
	log_trace(logTrace,"el archivo existe");
	//puts ("El archivo existe!\n");
	return 0;
}

//void setupFuseOperations(void){
//	oper.getattr = getattr;
////	oper.readdir = readdir;
//	oper.open    = open2;
//	oper.read    = read2;
//	oper.write   = write2;
//	oper.unlink  = unlink2;
//}



