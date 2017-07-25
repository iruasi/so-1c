#ifndef MANEJADORSADICA_H_
#define MANEJADORSADICA_H_

#include <commons/bitarray.h>

#include <tiposRecursos/tiposPaquetes.h>

#define DIR_METADATA "Metadata/"
#define BIN_METADATA "Metadata.bin"
#define DIR_BITMAP   "Metadata/Bitmap"
#define BIN_BITMAP   "Bitmap.bin"
#define DIR_ARCHIVOS "Archivos/"
#define DIR_BLOQUES  "Bloques/"

typedef struct{
	int size;
	char** bloques;
	char* ruta;
	int fd;
}tArchivos;

t_list* lista_archivos; //archivos que se vayan creando

typedef struct{
	int cantidad_bloques,
		tamanio_bloques;
	char* magic_number;
}tMetadata;

typedef struct{
	tPackHeader head;
	int rta; //devuelve el int correspondiente, en de falla devuelve el error
}tPackFSKer;

tArchivos* getInfoDeArchivo(char* path);
void escribirInfoEnArchivo(char* path, tArchivos* info);



void crearDirMontaje(void);

void crearArchivo(char* ruta);
void crearBloques(void);

int inicializarMetadata(void);
int inicializarBitmap(void);
t_bitarray* mapearBitArray(int fd);

void crearDirectoriosBase(void);
void crearDir(char *dir);
char* getPathFromFD(int fd);



#endif /* MANEJADORSADICA_H_ */
