#include <stdint.h>
#include <netdb.h>

#ifndef FILESYSTEM_H_
#define FILESYSTEM_H_

#endif /* FILESYSTEM_H_ */

#define MAX_IP_LEN 16   // aaa.bbb.ccc.ddd -> son 15 caracteres, 16 contando un '\0'
#define MAX_PORT_LEN 6  // 65535 -> 5 digitos, 6 contando un '\0'
#define MAXMSJ 100

#define FALLO_GRAL -21
#define FALLO_CONFIGURACION -22
#define FALLO_CONEXION -25

typedef struct{
	char* puerto;
	char* punto_montaje;
	char* ip_kernel;
}tFileSystem;

tFileSystem *getConfigFS(char*);
void mostrarConfiguracion(tFileSystem*);
void liberarConfiguracionFileSystem(tFileSystem*);
void setupHints (struct addrinfo*, int, int, int);
int establecerConexion(char*, char*);
