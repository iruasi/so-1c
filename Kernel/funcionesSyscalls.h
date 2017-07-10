#ifndef FUNCIONESSYSCALLS_H_
#define FUNCIONESSYSCALLS_H_


void waitSyscall(const char *sem, int pid);
void signalSyscall(const char *sem, int pid);

#endif /* FUNCIONESSYSCALLS_H_ */
