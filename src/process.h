#ifndef PROCESS_H
#define PROCESS_H

struct Process;

typedef void (*StdOutProc) (String str);

Process* create_process(String exe, String args = "", StdOutProc stdout_proc = nullptr);

#endif // PROCESS_H
