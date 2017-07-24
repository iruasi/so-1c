#include <fuse.h>
#include <sys/mman.h>

#include <commons/config.h>
#include <commons/bitarray.h>

#include <funcionesPaquetes/funcionesPaquetes.h>

#include "fileSystemConfigurators.h"
#include "manejadorSadica.h"
#include "operacionesFS.h"

extern tFileSystem *fileSystem;

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
	for(i=0; i<=meta->cantidad_bloques; i++){
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

void crearBitMap(){
	puts("Creando bitmap...");
	rutaBitMap = malloc(sizeof(fileSystem->punto_montaje) + sizeof(carpetaBitMap));
	memcpy(rutaBitMap, fileSystem->punto_montaje, sizeof(fileSystem->punto_montaje));
	memcpy(rutaBitMap+sizeof(fileSystem->punto_montaje), carpetaBitMap, sizeof(carpetaBitMap));
	FILE* bitmap = fopen(rutaBitMap, "wb");
	printf("Ruta del bitmap: %s\n", rutaBitMap);
	free(rutaBitMap);
}

void crearMetadata(){
	rutaMetadata = malloc(sizeof(fileSystem->punto_montaje) + sizeof(carpetaMetadata));
	memcpy(rutaMetadata, fileSystem->punto_montaje, sizeof(fileSystem->punto_montaje));
	memcpy(rutaMetadata+sizeof(fileSystem->punto_montaje), carpetaMetadata, sizeof(carpetaMetadata));
	printf("Ruta de la metadata: %s\n"
			"", rutaMetadata);
	FILE* metadataArch = fopen(rutaMetadata, "wb");
}

void crearDirMetadata(){
	char* rutaDirMetadata = malloc(sizeof(fileSystem->punto_montaje) + sizeof("/Metadata"));
	int off=0;
	memcpy(rutaDirMetadata, fileSystem->punto_montaje, sizeof(fileSystem->punto_montaje));
	off+=sizeof(fileSystem->punto_montaje);
	memcpy(rutaDirMetadata+off, "/Metadata", sizeof("/Metadata"));
	mkdir(rutaDirMetadata, S_IWUSR); //para que el usuario pueda escribir
	printf("Directorio %s creado!\n", rutaDirMetadata);
	free(rutaDirMetadata);
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

void escribirInfoEnArchivo(char* path, tArchivos* info){
	t_config* conf = config_create(path); // se trata como configs para sacar el tamanio y  bloque
	config_set_value(conf, "TAMANIO", (void*)info->size);
	config_set_value(conf, "BLOQUES", (void*)info->bloques);
	config_destroy(conf);
}

tArchivos* getInfoDeArchivo(char* path){
	t_config* conf = config_create(path);
	tArchivos* ret = malloc(sizeof(tArchivos));
	ret->size = config_get_int_value(conf, "TAMANIO");
	ret->bloques = config_get_array_value(conf, "BLOQUES");
	config_destroy(conf);
	return ret;
}

tMetadata* getInfoMetadata(char* path){
	t_config* conf = config_create(path);
	tMetadata* ret = malloc(sizeof(tMetadata));
	ret->cantidad_bloques = config_get_int_value(conf, "CANTIDAD_BLOQUES");
	ret->tamanio_bloques= config_get_int_value(conf, "TAMANIO_BLOQUES");
	ret->magic_number = config_get_string_value(conf, "MAGIC_NUMBER");
	config_destroy(conf);
	return ret;
}

void marcarBloquesOcupados(char* bloques[]){
	int i=0;
	while(i<sizeof(bloques)/sizeof(bloques[0])){
		bitarray_set_bit(bitArray, atoi(bloques[i])); //esto no me quedo claro si lo marca ocupado o libre, despues se cambia si no
		msync(bitArray, sizeof(t_bitarray), MS_SYNC);
		i++;
	}
}

t_bitarray* mapearBitArray(char* path){
	int fd = open(path, O_RDWR);
	struct stat mystat;

	/*if (fstat(fd, &mystat) < 0) {
	    printf("Error al establecer fstat\n");
	    close(fd);
	    return EXIT_FAILURE;
	}*/
	mapeado = mmap(NULL, mystat.st_size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
	t_bitarray* bitmap = bitarray_create(mapeado, sizeof(mapeado));
	return bitmap;
}

void escucharKernel(){
	char* bufHead = malloc(2*sizeof(int));
	char* bufRW = malloc(sizeof(tPackRW));
	char* bufAbrir = malloc(sizeof(tPackAbrir));
	char* bufBorrar = malloc(sizeof(tPackBytes));
	int stat, resultado;
	tPackHeader msj;
	tPackRW* rw = malloc(sizeof(tPackRW));
	tPackAbrir* abrir = malloc(sizeof(tPackAbrir));
	tPackBytes* borrar = malloc(sizeof(tPackBytes));
	struct fuse_file_info* fi; //se va a usar para pasar por parametro en las operaciones
	//todo: agregar header a los packs, o hacer dos sends/recvs entre ker y fs

	if((stat=recv(sock_kern, bufHead, 2*sizeof(int), 0))< 0){
		perror("Error al recibir mensaje de kernel...");
		return;
	}

	msj.tipo_de_proceso = KER;
	memcpy(&msj.tipo_de_mensaje, bufHead+sizeof(int), sizeof(int));

	/*
	 * TODO: (IMPORTANTE) inconsistencia de datos y parametros
	 * entre las operaciones del fs y los datos del pack
	 */
	switch(msj.tipo_de_mensaje){
	case LEER:
		recv(sock_kern, bufRW, sizeof(tPackRW), 0);
		rw = deserializeEscribir(bufRW);
		//resultado = read2(rw->path, rw->info, rw->tamanio, rw->offset, fi)
		break;
	case ABRIR:
		recv(sock_kern, bufAbrir, sizeof(tPackAbrir), 0);
		abrir = deserializeAbrir(bufAbrir);
		resultado = open2(abrir->direccion, fi);
		break;
	case BORRAR:
		recv(sock_kern, bufBorrar, sizeof(tPackBytes), 0);
		borrar = deserializeBytes(bufBorrar);
		resultado = unlink2(borrar->bytes);
		break;
	case ESCRIBIR:
		recv(sock_kern, bufRW, sizeof(tPackRW), 0);
		rw = deserializeEscribir(bufRW);
		//resultado = write2(getPathFromFD(rw->fd), rw->info, rw->tamanio, rw->offset, fi);
		break;
	default:
		perror("Operacion no reconocida...");
		break;
	}

	free(bufHead);
	free(bufRW);
	free(bufAbrir);
	free(bufBorrar);
	free(rw);
	free(abrir);
	free(borrar);
}

char* getPathFromFD(int fd){
	int i=0;
	tArchivos* arch = malloc(sizeof(tArchivos));
	while(i<list_size(lista_archivos)){
		arch = list_get(lista_archivos, i);
		if(arch->fd==fd){
			return arch->ruta;
		}
	}
	free(arch);
	return "NOT FOUND";
}
