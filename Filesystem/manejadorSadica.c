#include <fuse.h>
#include <sys/mman.h>
#include <errno.h>

#include <commons/config.h>
#include <commons/bitarray.h>
#include <commons/string.h>
#include <commons/log.h>

#include <funcionesPaquetes/funcionesPaquetes.h>

#include "fileSystemConfigurators.h"
#include "manejadorSadica.h"
#include "operacionesFS.h"

t_log *logTrace;

tMetadata* meta;
t_bitarray* bitArray;

char *mapeado;
char *rutaMetadata, *binMetadata_path, *binBitmap_path;
FILE *metadataArch, *bitmapArch;

int sock_kern;
extern tFileSystem *fileSystem;

int crearArchivo(char* ruta){

	char *path = string_duplicate(fileSystem->punto_montaje);
	string_append(&path, "Archivos"); string_append(&path, ruta);

	if(validarArchivo(ruta)== -1){
		perror("El archivo no existe");
		log_error(logTrace, "El archivo no existe");
	}
	if ((open(path, O_RDWR | O_CREAT, 0777)) == -1){
		perror("No se pudo crear el archivo. error");
		log_error(logTrace,"no se pudo crear el archivo");
		return FALLO_GRAL;
	}
	log_trace(logTrace,"se creo el archivo %s",path);

	iniciarBloques(1, path);
	free(path);
	return 0;
}

int inicializarMetadata(void){
	//puts("Inicializando metadata...");
	log_trace(logTrace,"inicializando metadata");
	rutaMetadata = string_duplicate(fileSystem->punto_montaje);
	string_append(&rutaMetadata, DIR_METADATA);
	binMetadata_path = string_duplicate(rutaMetadata);
	string_append(&binMetadata_path, BIN_METADATA);

	if ((metadataArch = fopen(binMetadata_path, "rb")) == NULL){
		log_error(logTrace,"no s epudo abrir el archivo");
		perror("No se pudo abrir el archivo. error");
		return FALLO_GRAL;
	}

	t_config *meta_bin     = config_create(binMetadata_path);
	meta                   = malloc(sizeof *meta);
	meta->magic_number     = string_new();
	meta->cantidad_bloques = config_get_int_value(meta_bin, "CANTIDAD_BLOQUES");
	meta->tamanio_bloques  = config_get_int_value(meta_bin, "TAMANIO_BLOQUES");
	string_append(&meta->magic_number, config_get_string_value(meta_bin, "MAGIC_NUMBER"));
	log_trace(logTrace,"se levanto correctamente el archivo %s",binMetadata_path);
	//printf("Se levanto correctamente el archivo %s\n", binMetadata_path);
	config_destroy(meta_bin);
	return 0;
}

int inicializarBitmap(void){
	//puts("Inicializando bitmap...");
	log_trace(logTrace,"inicializando bitmap");
	int fd;
	binBitmap_path = string_duplicate(rutaMetadata);
	string_append(&binBitmap_path, BIN_BITMAP);

	if ((fd = open(binBitmap_path, O_RDWR)) == -1){
		perror("No se pudo crear el archivo. error");
		return FALLO_GRAL;
	}
	//printf("Se creo el archivo %s\n", binBitmap_path);
	log_trace(logTrace,"se creo el archivo %s",binBitmap_path);
	bitArray = mapearBitArray(fd);
	msync(bitArray->bitarray, bitArray->size, bitArray->mode);
	return 0;
}

t_bitarray* mapearBitArray(int fd){

	struct stat bmpstat;
	if (fstat(fd, &bmpstat) < 0){
		log_error(logTrace,"fallo seteo del stat del bitmap");
		perror("Fallo seteo de stat del bitmap. error");
		return NULL;
	}

	mapeado = mmap(NULL, bmpstat.st_size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
	return bitarray_create_with_mode(mapeado, meta->cantidad_bloques, LSB_FIRST);
}

void crearDirMontaje(void){

	if (mkdir(fileSystem->punto_montaje, 0777) == -1){
		perror("No se pudo crear el directorio. error");
		log_error(logTrace,"no se pudo crear el dire;path intentado: %s",fileSystem->punto_montaje);
		//printf("Path intentado: %s\n", fileSystem->punto_montaje);
	}
}

void crearDirectoriosBase(void){
	//puts("Se recrea el arbol principal de directorios");
	log_trace(logTrace,"se recrea el arbol principal de directorios");
	crearDir(DIR_METADATA);
	crearDir(DIR_ARCHIVOS);
	crearDir(DIR_BLOQUES);
}

void crearDir(char *dir){
	//printf("Se intenta crear el directorio %s\n", dir);
	log_trace(logTrace,"se intenta crear el directorio");
	char* rutaDir = string_duplicate(fileSystem->punto_montaje);
	string_append(&rutaDir, dir);

	if (mkdir(rutaDir, 0777) != 0){ //para que el usuario pueda escribir
		if (errno != EEXIST){
			log_error(logTrace,"no se pudo crear directorio");
			perror("No se pudo crear directorio. error:");
			return;
		}
		log_trace(logTrace,"ya existia la ruta");
		//printf("Ya existia la ruta %s\n", rutaDir);
		return;
	}
	log_trace(logTrace, "Directorio %s creado", rutaDir);
	free(rutaDir);
}

void escribirInfoEnArchivo(char* path, tArchivos* info){
	log_trace(logTrace, "Escribir info en archivo");

	t_config* conf = config_create(path); // se trata como configs para sacar el tamanio y  bloque
	char str_size[MAX_DIG];
	sprintf(str_size,   "%d", info->size);
	config_set_value(conf, "TAMANIO", str_size);
	char *array = crearStringListaBloques(info->bloques);
 	config_set_value(conf, "BLOQUES", array);

 	free(array);
	config_save(conf);
	config_destroy(conf);
}

char *crearStringListaBloques(char **bloques){

	int len = sizeof(bloques) / sizeof(bloques[0]);

	char *block_arr = string_new();
	string_append(&block_arr, "[");

	int i;
	for (i = 0; i < len; ++i){
		string_append(&block_arr, bloques[i]);
		if (i == len - 1)
			break;
		string_append(&block_arr, ", ");
	}


	string_append(&block_arr, "]");
	return block_arr;
}

tArchivos* getInfoDeArchivo(char* path){
	t_config* conf = config_create(path);
	tArchivos* ret = malloc(sizeof(tArchivos));
	ret->size = config_get_int_value(conf, "TAMANIO");
	ret->bloques = config_get_array_value(conf, "BLOQUES");
	config_destroy(conf);
	return ret;
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
