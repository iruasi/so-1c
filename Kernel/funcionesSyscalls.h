#ifndef FUNCIONESSYSCALLS_H_
#define FUNCIONESSYSCALLS_H_


void waitSyscall(const char *sem, int pid);
void signalSyscall(const char *sem, int pid);

int setGlobalSyscall(tPackValComp *val_comp);
t_valor_variable getGlobalSyscall(t_nombre_variable *var, bool* found);

#endif /* FUNCIONESSYSCALLS_H_ */
