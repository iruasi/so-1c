#ifndef MANEJADORSADICA_H_
#define MANEJADORSADICA_H_

#include <commons/bitarray.h>

#include <tiposRecursos/tiposPaquetes.h>

#define carpetaBitMap "/Metadata/Bitmap.bin"
#define carpetaMetadata "/Metadata/Metadata.bin"


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



//todo: estos conviene dejarlos como global o en la struct tfilesystem?
tMetadata* meta;
t_bitarray* bitArray;

char* mapeado;
char* rutaBitMap;
char* rutaMetadata;

int sock_kern;

void crearArchivo(char* ruta);
void crearBloques(void);
void crearMetadata(void);
void crearDirMetadata(void);
void crearDirArchivos(void);
void crearDirBloques(void);

char* getPathFromFD(int fd);



#endif /* MANEJADORSADICA_H_ */
