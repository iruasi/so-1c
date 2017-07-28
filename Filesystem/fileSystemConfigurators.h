#ifndef FILESYSTEM_H_
#define FILESYSTEM_H_

#include <stdint.h>
#include <netdb.h>

#include <tiposRecursos/tiposErrores.h>

#define MAX_IP_LEN 16   // aaa.bbb.ccc.ddd -> son 15 caracteres, 16 contando un '\0'
#define MAX_PORT_LEN 6  // 65535 -> 5 digitos, 6 contando un '\0'
#define MAX_DIG 6
#define MAXMSJ 100
#define MAX_MNTLEN 30


typedef struct{
	char* puerto_entrada;
	char* punto_montaje;
	char* ip_kernel;
	uint32_t tipo_de_proceso;
}tFileSystem;

tFileSystem *getConfigFS(char*);
void mostrarConfiguracion(tFileSystem*);
void liberarConfiguracionFileSystem(tFileSystem*);
void setupHints (struct addrinfo*, int, int, int);
int establecerConexion(char*, char*);

#endif /* FILESYSTEM_H_ */
