#ifndef MANEJADORSADICA_H_
#define MANEJADORSADICA_H_

#define carpetaBitMap "/Metadata/Bitmap.bin"
#define carpetaMetadata "/Metadata/Metadata.bin"

typedef struct{
	int size;
	char** bloques;
	char* ruta;
}tArchivos;

typedef struct{
	int cantidad_bloques,
		tamanio_bloques;
	char* magic_number;
}tMetadata;

//todo: estos conviene dejarlos como global o en la struct tfilesystem?
tMetadata* meta;
t_bitarray* bitArray;

char* mapeado;
char* rutaBitMap;
char* rutaMetadata;

void crearArchivo(char* ruta);
void crearBloques(void);
void crearMetadata(void);
void crearDirMetadata(void);
void crearDirArchivos(void);
void crearDirBloques(void);



#endif /* MANEJADORSADICA_H_ */
