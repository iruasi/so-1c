#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>

#include "auxiliaresKernel.h"

#include "../Compartidas/funcionesPaquetes.h"
#include "../Compartidas/tiposErrores.h"
#include "../Compartidas/tiposPaquetes.h"

int recibirCodYReenviar(tPackHeader *head, int fd_sender, int fd_mem){

	int stat;
	tProceso proc = KER;

	int packageSize; // aca guardamos el tamanio total del paquete serializado

	void *pack_src_serial = serializeSrcCodeFromRecv(fd_sender, *head, &packageSize);
	if (pack_src_serial == NULL){
		puts("Fallo al recibir y serializar codigo fuente");
		return FALLO_GRAL;
	}

	memcpy(pack_src_serial, &proc, sizeof proc);

	if ((stat = send(fd_mem, pack_src_serial, packageSize, 0)) < 0){
		perror("Error en el envio de codigo fuente. error");
		return FALLO_SEND;
	}

	free(pack_src_serial);
	return 0;
}
