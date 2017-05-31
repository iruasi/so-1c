#ifndef AUXILIARES_H_
#define AUXILIARES_H_

#include <tiposRecursos/tiposPaquetes.h>

tPackSrcCode *readFileIntoPack(tProceso sender, char* ruta);
void *serializarSrcCode(tPackSrcCode *src_code);


#endif // AUXILIARES_H_
