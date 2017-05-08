#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>

#include "tPacks.c"
#include "tiposErrores.h"

#define HEAD_SIZE 8

/* dado un socket e identificador de proceso, le envia un paquete basico de HandShake
 */
int handshakeCon(int sock_dest, int id_sender){

	tPackHeader head;
	head.tipo_de_proceso = id_sender;
	head.tipo_de_mensaje = HSHAKE;

	void *package = malloc(HEAD_SIZE);
	memcpy(package, &head, HEAD_SIZE);

	return send(sock_dest, package, HEAD_SIZE, 0);
}

/* recibimos codigo fuente del socket de entrada
 * devolvemos un puntero a memoria que lo contiene
 */
void * recvSourceCode(int sock_in, tProceso sender){
	tPackSrcCode *srcCode;
	int stat;
	int payload_size;

	if ((stat = recv(sock_in, &payload_size, sizeof (uint32_t), 0)) <= 0){
		perror("El socket cerro la conexion o hubo fallo de recepcion. error");
		errno = FALLO_RECV;
		return NULL;
	}

	srcCode->sourceCode = malloc(payload_size);

	if ((stat = recv(sock_in, srcCode->sourceCode, payload_size, 0)) <= 0){
		perror("El socket cerro la conexion o hubo fallo de recepcion. error");
		errno = FALLO_RECV;
		return NULL;
	}

	return srcCode;
}









