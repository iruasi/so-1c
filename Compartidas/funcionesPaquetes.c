#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>

#include "tiposPaquetes.h"
#include "tiposErrores.h"


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
void *recvSourceCode(int sock_in){

	int stat;
	unsigned long srcCode_size;
	tPackSrcCode *src_pack = malloc(sizeof *src_pack);

	// offset hasta donde comienza el codigo fuente posta
	size_t source_off = sizeof src_pack->head + sizeof src_pack->sourceLen;

	if ((stat = recv(sock_in, &srcCode_size, sizeof (unsigned long), 0)) <= 0){
		perror("El socket cerro la conexion o hubo fallo de recepcion. error");
		errno = FALLO_RECV;
		return NULL;
	}


	src_pack->sourceLen = srcCode_size;
	// extendemos el size de srcCode, para que entre el codigo enviado posta
	src_pack = realloc(src_pack, sizeof src_pack->head + sizeof src_pack->sourceLen + srcCode_size);
	src_pack->sourceCode = (char *) (uint32_t) src_pack + source_off;

	// todo: por algun motivo misterioso, si no hacemos alguna asignacion arbitraria al sourceCode, su escritura a memoria falla...
	*src_pack->sourceCode = 'C';

	if ((stat = recv(sock_in, src_pack->sourceCode, srcCode_size, 0)) <= 0){
		perror("El socket cerro la conexion o hubo fallo de recepcion. error");
		errno = FALLO_RECV;
		return NULL;
	}

	return src_pack;
}
