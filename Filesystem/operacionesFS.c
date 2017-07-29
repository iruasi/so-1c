#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <sys/mman.h>

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

char *read2(char *path, size_t size, off_t offset, int *bytes_read) {
	log_trace(logTrace,"se quiere leer el archivo %s size:%d offset:%d",path,size,offset);

	if(validarArchivo(path) == -1){
		log_error(logTrace, "error al leer el archivo");
		perror("Error al leer el archivo...");
		return NULL;
	}

	char *info;
	tArchivos* file = getInfoDeArchivo(path);
	tFileBLKS *arch = getFileByPath(path);

	info = leerInfoDeBloques(file->bloques, arch->blk_quant, size, offset, bytes_read);
	if ((*(unsigned int *)bytes_read) < size){
		log_error(logTrace, "Fallo lectura de informacion. Size pedido: %d, Leido: %d", size, *bytes_read);
		free(info); // liberamos la info a medio masticar, retornamos NULL
		return NULL;
	}

	log_trace(logTrace,"Se leyeron los datos %s", info);
	return info;
}

int write2(char * abs_path, char * buf, size_t size, off_t offset){
	log_trace(logTrace, "Escribir %d bytes en %s", size, abs_path);

	int blocks_req, size_efectivo;
	char **bloques;
	tArchivos* file = getInfoDeArchivo(abs_path);
	tFileBLKS *arch = getFileByPath(abs_path);

	size_efectivo = offset + size - file->size;
	blocks_req = ceil((float) (offset + size - file->size) / meta->tamanio_bloques);

	if ((bloques = obtenerBloquesDisponibles(blocks_req)) == NULL){
		log_error(logTrace, "no alcanzan los bloques libres. No pudo escribir");
		return FALLO_ESCRITURA;
	}

	marcarBloquesOcupados(bloques, arch->blk_quant);
	if (agregarBloquesSobreBloques(&file->bloques, bloques, arch->blk_quant, blocks_req) < 0){
		log_error(logTrace, "No se pudo agregar los bloques nuevos  necesarios al archivo");
		return FALLO_GRAL;
	}
	arch->blk_quant += blocks_req;

	if (escribirInfoEnBloques(file->bloques, buf, size, offset) < 0){
		log_error(logTrace, "No se pudo escribir la informacion requerida");
		return FALLO_ESCRITURA;
	}

	file->size = size_efectivo;
	escribirInfoEnArchivo(abs_path, file);

	log_trace(logTrace,"Se escribieron los datos en el archivo");
	free(file->bloques);
	free(file);
	return size_efectivo;
}

char *leerInfoDeBloques(char **bloques, int blk_quant, size_t size, off_t off, int *ioff){

	char *bloq_path = string_duplicate(fileSystem->punto_montaje);
	string_append(&bloq_path, "Bloques/");

	char *buff = malloc(size);

	int start_b    = off / meta->tamanio_bloques;
	int start_off  = off % meta->tamanio_bloques;
	*ioff = 0; // offset sobre la info ya leida

	char sblk[MAXMSJ];
	sprintf(sblk, "%s%s.bin", bloq_path, bloques[start_b]);

	// primera lectura!
	if (leerBloque(sblk, start_off, &buff, meta->tamanio_bloques - start_off) < 0){
		log_error(logTrace, "Fallo lectura de Bloque");
		return buff;
	}
	size -= meta->tamanio_bloques - start_off;
	*ioff += meta->tamanio_bloques - start_off;

	while (size){
		start_b++;
		sprintf(sblk, "%s%s.bin", bloq_path, bloques[start_b]);
		buff += *ioff;
		if (leerBloque(sblk, start_off, &buff , size) < 0){
			log_error(logTrace, "Fallo lectura de Bloque");
			return buff;
		}
		*ioff += MIN(meta->tamanio_bloques, (int) size);
		size -= MIN(meta->tamanio_bloques, (int) size);
	}
	return buff;
}

int leerBloque(char *sblk, off_t off, char **info, int rd_size){

	FILE *f;
	int valid_size = MIN(rd_size, meta->tamanio_bloques);

	if ((f = fopen(sblk, "rb")) == NULL){
		perror("Fallo fopen");
		log_error(logTrace, "Fallo fopen de %s", sblk);
		return FALLO_GRAL;
	}
	if ((fseek(f, off, SEEK_SET)) == -1){
		perror("Fallo fseek a File. error");
		log_error(logTrace, "Fallo fseek %d sobre %s", off, sblk);
		return FALLO_LECTURA;
	}
	if (fread(*info, valid_size, 1, f) < 1){
		perror("Fallo fwrite. error");
		log_error(logTrace, "Fallo fread de %d bytes sobre %s", valid_size, sblk);
		return FALLO_LECTURA;
	}

	if (fclose(f) != 0){
		perror("Fallo fclose. error");
		log_error(logTrace, "Fallo fclose", valid_size, sblk);
		return FALLO_GRAL;
	}
	return 0;
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
	if (escribirBloque(sblk, start_off, info, meta->tamanio_bloques - start_off) < 0){
		log_error(logTrace, "Fallo escritura de Bloque");
		return FALLO_ESCRITURA;
	}
	size -= meta->tamanio_bloques - start_off;
	ioff += meta->tamanio_bloques - start_off;

	while (size){
		start_b++;
		sprintf(sblk, "%s%s.bin", bloq_path, bloques[start_b]);

		if (escribirBloque(sblk, 0, info + ioff, size) < 0){
			log_error(logTrace, "Fallo escritura de Bloque");
			return FALLO_ESCRITURA;
		}
		size -= MIN(meta->tamanio_bloques, (int) size);
		ioff += MIN(meta->tamanio_bloques, (int) size);
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
	log_trace(logTrace, "Se inicializan %d bloques para %s", cant, path);

	char **bloquesArch;
	int i;

	if((bloquesArch = obtenerBloquesDisponibles(cant)) == NULL){
		log_error(logTrace, "No se pudieron obtener bloques disponibles");
		return;
	}

	for(i = 0; i < cant; i++)
		bitarray_set_bit(bitArray, atoi(bloquesArch[i]));
	msync(bitArray->bitarray, bitArray->size, bitArray->mode);

	tArchivos* info = malloc(sizeof (tArchivos));
	info->bloques      = bloquesArch;
	info->size = 0;

	escribirInfoEnArchivo(path, info);
	free(info->bloques);
	free(info);
}

char **obtenerBloquesDisponibles(int cant){
	log_trace(logTrace, "Obtener bloques disponibles");
	int i, pos_blk;
	char **bloques = malloc(cant * sizeof(char*));
	for(i=0; i<cant; i++)
		bloques[i] = malloc(MAX_DIG + 1);

	int resto = cant;
	int libres = obtenerCantidadBloquesLibres();
	char snum[MAX_DIG];

	if(libres < cant){
		log_error(logTrace,"no hay bloques suficientes para escribir los datos");
		perror("No hay bloques suficientes para escribir los datos");
		return NULL;
	}

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
int agregarBloquesSobreBloques(char ***bloques, char **blq_add, int prev, int add){
	log_trace(logTrace, "Se agregan los %d nuevos bloques", add);
	int i, j;

	if (realloc(*bloques, (prev + add) * sizeof(char*)) == NULL){
		log_error(logTrace, "Fallo reallocacion de bloques");
		return FALLO_GRAL;
	}

	for(j = 0, i = prev; i < (prev + add); i++, j++){
		if(((*bloques)[i] = malloc(MAX_DIG + 1)) == NULL){
			log_error(logTrace, "Fallo mallocacion de bloque");
			return FALLO_GRAL;
		}
		memcpy((*bloques)[i], blq_add[j], MAX_DIG + 1);
	}
	return 0;
}

void marcarBloquesOcupados(char **bloques, int cant){

	int i;
	for (i = 0; i < cant; ++i)
		bitarray_set_bit(bitArray, atoi(bloques[i]));
	msync(bitArray->bitarray, bitArray->size, bitArray->mode);
}

int obtenerCantidadBloquesLibres(void){
	log_trace(logTrace, "Obtener cantidad de bloques libres");

	int i;
	int libres=0;
	for(i=0; i< meta->cantidad_bloques; i++){
		if(!bitarray_test_bit(bitArray, i))
			libres++;
	}
	return libres;
}

int unlink2 (char *path){ // todo: hacer
	log_trace(logTrace,"se quiere borrar el archivo %s", path);

	log_trace(logTrace,"se quiere borrar el archivo %s",path);
	//printf("Se quiere borrar el archivo el archivo %s\n", path);
	if(validarArchivo(path)==-1){
		log_error(logTrace,"error al borrar archivo");
		perror("Error al borrar archivo");
		return -1;
	}

	tArchivos* archivo = getInfoDeArchivo(path);
	t_config* conf = config_create(path);
	int i;
	for(i = 0; i < sizeof(archivo->bloques)/sizeof(archivo->bloques[0]); i++)
		bitarray_clean_bit(bitArray, atoi(archivo->bloques[i]));
	msync(bitArray->bitarray, bitArray->size, bitArray->mode);

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
