#ifndef FILESYSTEM_H_
#define FILESYSTEM_H_

#define MAX_IP_LEN 16   // aaa.bbb.ccc.ddd -> son 15 caracteres, 16 contando un '\0'
#define MAX_PORT_LEN 6  // 65535 -> 5 digitos, 6 contando un '\0'
#define MAXMSJ 100
#include "../Compartidas/tiposErrores.h"

typedef struct{
	char* puerto_entrada;
	char* punto_montaje;
	char* ip_kernel;
}tFileSystem;

tFileSystem *getConfigFS(char*);
void mostrarConfiguracion(tFileSystem*);
void liberarConfiguracionFileSystem(tFileSystem*);

#endif /* FILESYSTEM_H_ */
