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

//todo: estos conviene dejarlos como global o en la struct tfilesystem?
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

	if ((open(path, O_RDWR | O_CREAT, 0777)) == -1){
		perror("No se pudo crear el archivo. error");
		log_error(logTrace,"no se pudo crear el archivo");
		return FALLO_GRAL;
	}
	log_trace(logTrace,"se creo el archivo %s",path);

//	if (!asignarBloques(1)){ // todo: le asingamos al menos 1 bloque
//		free(path);
//		return FALLO_GRAL;
//	}
	return 0;
}

int crearBloques(void){
	log_trace(logTrace,"creando bloques");

	int off=0;
	int i;
	for(i = 0; i <= meta->cantidad_bloques; i++){
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

	return 0;
}

void crearBitMap(void){
	log_trace(logTrace,"creando bitmap");
	//puts("Creando bitmap...");
	return;
//	int len_mntpnt = strlen(fileSystem->punto_montaje);
//	int len_dirbmp = strlen(carpetaBitMap);
//	rutaBitMap = malloc(len_mntpnt + len_dirbmp);
//	memcpy(rutaBitMap, fileSystem->punto_montaje, len_mntpnt);
//	memcpy(rutaBitMap + len_mntpnt - 1, carpetaBitMap, len_dirbmp);
//	FILE* bitmap = fopen(rutaBitMap, "wb");
//	printf("Ruta del bitmap: %s\n", rutaBitMap);
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
	log_trace(logTrace,"direcotrio %s creado",rutaDir);
	//printf("Directorio %s creado\n", rutaDir);
	free(rutaDir);
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

void marcarBloquesOcupados(char* bloques[]){
	int i=0;
	while(i < sizeof(bloques)/sizeof(bloques[0])){

		bitarray_set_bit(bitArray, atoi(bloques[i]));
		if (!msync(bitArray, sizeof(t_bitarray), MS_SYNC)){
			log_error(logTrace,"fallo syn del bitarray");
			perror("Fallo sync del bitArray. error");
		}
		i++;
	}
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
		log_trace(logTrace,"error al recibir msj de kernel");
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
		log_error(logTrace,"operacion no reconocida");
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
