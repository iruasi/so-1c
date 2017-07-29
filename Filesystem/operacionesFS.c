#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>

#include <commons/log.h>
#include <commons/collections/list.h>
#include <commons/config.h>

#include "fileSystemConfigurators.h"
#include "operacionesFS.h"
#include "manejadorSadica.h"

extern t_bitarray *bitArray;
extern tMetadata *meta;
extern tFileSystem* fileSystem;
extern t_log *logTrace;


int read2(char *path, char **buf, size_t size, off_t offset) {

	log_trace(logTrace,"se quiere leer el archivo %s size:%d offset:%d",path,size,offset);
	//printf("Se quiere leer el archivo %s, size: %d, offset: %d\n", path, size, offset);
	if(validarArchivo(path)==-1){
		log_error(logTrace,"error al leer el archivo");
		perror("Error al leer el archivo...");
		return -1;
	}
	fseek(path, offset, SEEK_SET);
	fread(*buf, size, 1, path);
	//printf("Datos leidos: %s\n", *buf);
	log_trace(logTrace,"datos leidos %s",buf);
	return size;
}

int write2(char * abs_path, char * buf, size_t size, off_t offset){
	log_trace(logTrace, "Escribir %d bytes en %s", size, abs_path);

	int blocks_req, size_efectivo;
	char **bloques;
	tArchivos* file = getInfoDeArchivo(abs_path);

	size_efectivo = offset + size - file->size;
	blocks_req = ceil((float) (offset + size - file->size) / meta->tamanio_bloques);

	if ((bloques = obtenerBloquesDisponibles(blocks_req)) == NULL){
		log_error(logTrace, "no alcanzan los bloques libres. No pudo escribir");
		return FALLO_ESCRITURA;
	}
	marcarBloquesOcupados(bloques);
	agregarBloquesSobreBloques(&file->bloques, bloques);

	escribirInfoEnBloques(file->bloques, buf, size, offset);

	file->size = size_efectivo;
	escribirInfoEnArchivo(abs_path, file);
	log_trace(logTrace,"se escribieron los datos en el archivo");
	free(file->bloques);
	free(file);
	return size_efectivo;
}

int escribirInfoEnBloques(char **bloques, char *info, size_t size, off_t off){

	char *bloq_path = string_duplicate(fileSystem->punto_montaje);
	string_append(&bloq_path, "Bloques/");

	int start_b    = off / meta->tamanio_bloques;
	int start_off  = off % meta->tamanio_bloques;
	int ioff = 0; // offset sobre la info ya escrita

	char sblk[MAXMSJ];
	sprintf(sblk, "%s%s.bin", bloq_path, bloques[start_b]);

	// primera escritura!
	escribirBloque(sblk, start_off, info, meta->tamanio_bloques - start_off);
	ioff += start_off;

	while (size){
		start_b++;
		sprintf(sblk, "%s%s.bin", bloq_path, bloques[start_b]);

		if(escribirBloque(sblk, 0, info, size) < 0){
			log_error(logTrace, "Fallo escritura de Bloque");
			return FALLO_ESCRITURA;
		}
		size -= MIN(meta->tamanio_bloques, (int) size);
	}
	return 0;
}

int escribirBloque(char *sblk, off_t off, char *info, int wr_size){

	FILE *f;
	int valid_size = MIN(wr_size, meta->tamanio_bloques);

	if ((f = fopen(sblk, "wb")) == NULL){
		perror("Fallo fopen");
		log_error(logTrace, "Fallo fopen de %s", sblk);
		return FALLO_GRAL;
	}
	if ((fseek(f, off, SEEK_SET)) == -1){
		perror("Fallo fseek a File. error");
		log_error(logTrace, "Fallo fseek %d sobre %s", off, sblk);
		return FALLO_ESCRITURA;
	}
	if (fwrite(info, valid_size, 1, f) < 1){
		perror("Fallo fwrite. error");
		log_error(logTrace, "Fallo fwrite de %d bytes sobre %s", valid_size, sblk);
		return FALLO_ESCRITURA;
	}

	if (fclose(f) != 0){
		perror("Fallo fclose. error");
		log_error(logTrace, "Fallo fclose", valid_size, sblk);
		return FALLO_GRAL;
	}
	return 0;
}

void iniciarBloques(int cant, char* path){

	char **bloquesArch;
	uint32_t i;

	if((bloquesArch = obtenerBloquesDisponibles(cant)) == NULL){
		log_error(logTrace, "No se pudieron obtener bloques disponibles");
		return;
	}

	//log_trace(logTrace, "Se marcan los bloques ocupados");
	for(i=0; i < sizeof(bloquesArch)/sizeof(bloquesArch[0]); i++)
		bitarray_set_bit(bitArray, atoi(bloquesArch[i]));

	tArchivos* info = malloc(sizeof (tArchivos));
	info->bloques = bloquesArch;
	info->size = 0;

	escribirInfoEnArchivo(path, info);
	free(info->bloques);
	free(info);
}

char **obtenerBloquesDisponibles(int cant){
	log_trace(logTrace, "Obtener bloques disponibles");

	int i, pos_blk;
	int resto = cant;
	int libres = obtenerCantidadBloquesLibres();
	char snum[MAX_DIG];

	if(libres < cant){
		log_error(logTrace,"no hay bloques suficientes para escribir los datos");
		perror("No hay bloques suficientes para escribir los datos");
		return NULL;
	}
	char **bloques = malloc(cant * sizeof(char*));
	for(i=0; i<cant; i++)
		bloques[i] = malloc(MAX_DIG + 1);

	i = pos_blk = 0;
	while(i < meta->cantidad_bloques && resto > 0){
		if(!bitarray_test_bit(bitArray, i)){
			sprintf(snum, "%d", i);
			strcpy(bloques[pos_blk], snum);
			--resto; ++pos_blk;
		}
		 ++i;
	}
	return bloques;
}

/* bloques llega con el formato ["1", "2", "3"] */
void agregarBloquesSobreBloques(char ***bloques, char **blq_add){

	int i, j;
	int len     = sizeof(bloques) / sizeof(bloques[0]);
	int len_add = sizeof(blq_add) / sizeof(blq_add[0]);

	realloc(*bloques, (len + len_add) * sizeof(char*));
	for(j = 0, i = len_add; i < (len + len_add); i++, j++){
		(*bloques)[i] = malloc(MAX_DIG + 1);

		memcpy((*bloques)[i], blq_add[j], MAX_DIG + 1);
	}
}

void marcarBloquesOcupados(char **bloques){

	int i;
	int len = sizeof(bloques) / sizeof(bloques[0]);
	for (i = 0; i < len; ++i)
		bitarray_set_bit(bitArray, atoi(bloques[i]));
}

int obtenerCantidadBloquesLibres(void){
	log_trace(logTrace, "Obtener cantidad de bloques libres");

	int i,libres;
	for(i=libres=0; i< meta->cantidad_bloques; i++){
		if(!bitarray_test_bit(bitArray, i))
			libres++;
	}
	return libres;
}

int unlink2 (char *path){
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
