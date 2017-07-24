#ifndef MANEJADORES_H_
#define MANEJADORES_H_

void setupGlobales_manejadores(void);

void cpu_manejador(void *cpuInfo);
void mem_manejador(void *m_sock);
void cons_manejador(void *conInfo);

#endif
