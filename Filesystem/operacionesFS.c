#include <stddef.h>
#include <stdlib.h>
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include "operacionesFS.h"
//todo: ver como usar los bitarray y el mmap

static int getattr(const char *path, struct stat *stbuf) {
	int res = 0;

	memset(stbuf, 0, sizeof(struct stat));

	//Si path es igual a "/" nos estan pidiendo los atributos del punto de montaje

	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else if (strcmp(path, DEFAULT_FILE_PATH) == 0) {
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = strlen(DEFAULT_FILE_CONTENT);
	} else {
		res = -ENOENT;
	}
	return res;
}

static int readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
	(void) offset;
	(void) fi;

	if (strcmp(path, "/") != 0)
		return -ENOENT;

	// "." y ".." son entradas validas, la primera es una referencia al directorio donde estamos parados
	// y la segunda indica el directorio padre
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	filler(buf, DEFAULT_FILE_NAME, NULL, 0);
	printf("Leyendo los archivos de %s\n", path);

	return 0;
}

static int open2(const char *path, struct fuse_file_info *fi) {
	if (strcmp(path, DEFAULT_FILE_PATH) != 0)
		return -ENOENT;

	if ((fi->flags & 3) != O_RDONLY)
		return -EACCES;
	puts ("Se abre un archivo");
	fopen(path, "wb"); //todo: ver como manejar las banderas


	return 0; //todo: ver como retornar el fd
}

static int read2(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	size_t len;
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
	return size;
}

static int write2(const char * path, const char * buf, size_t size, off_t offset, struct fuse_file_info * fi){
	printf("Escribir en %s los datos de %s el tamanio %d", path, buf, size);
	fwrite(buf, size, offset, (void*)path);
	return size;

}

static int unlink2 (const char *path){
	int rem;
	printf("Se quiere borrar el archivo el archivo %s\n", path);
	if((rem=remove(path))==-1){
		puts("Error al borrar el archivo");
		return 1;
	}
	else puts ("Se borro el archivo");
	return 0;
	//todo: me tira error pero borra el archivo, ver por que
}

/*
 * Esta es la estructura principal de FUSE con la cual nosotros le decimos a
 * biblioteca que funciones tiene que invocar segun que se le pida a FUSE.
 * Como se observa la estructura contiene punteros a funciones.
 */

void setupFuseOperations(void){
	oper.getattr = getattr;
//	oper.readdir = readdir;
	oper.open = open2;
	oper.read = read2;
	oper.write = write2;
	oper.unlink = unlink2;
}

//TODO: pasar al main
// Dentro de los argumentos que recibe nuestro programa obligatoriamente
// debe estar el path al directorio donde vamos a montar nuestro FS
//int main(int argc, char *argv[]) {
//	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
//
//	// Limpio la estructura que va a contener los parametros
//	memset(&runtime_options, 0, sizeof(struct t_runtime_options));
//
//	// Esta funcion de FUSE lee los parametros recibidos y los intepreta
//	if (fuse_opt_parse(&args, &runtime_options, fuse_options, NULL) == -1){
//		/** error parsing options */
//		perror("Invalid arguments!");
//		return EXIT_FAILURE;
//	}
//
//	// Si se paso el parametro --welcome-msg
//	// el campo welcome_msg deberia tener el
//	// valor pasado
//	if( runtime_options.welcome_msg != NULL ){
//		printf("%s\n", runtime_options.welcome_msg);
//	}
//
//	// Esta es la funcion principal de FUSE, es la que se encarga
//	// de realizar el montaje, comuniscarse con el kernel, delegar todo
//	// en varios threads
//	FILE* bloq1 = fopen("1.bin", wb); //todo: en el main se va a crear en la ruta que esta en el config
//	return fuse_main(args.argc, args.argv, &oper, NULL);
//}


