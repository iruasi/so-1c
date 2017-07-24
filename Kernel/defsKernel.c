/*
 * Implementa las cosas que hay en defsKernel.h
 */

#include "defsKernel.h"
#include "capaMemoria.h"
#include "auxiliaresKernel.h"
#include "funcionesSyscalls.h"

void setupVariablesPorSubsistema(void){

	setupGlobales_capaMemoria();
	setupGlobales_auxiliares();
	setupGlobales_syscalls();
 // todo: si
}
