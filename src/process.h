#ifndef PROCESS_H
#define PROCESS_H

struct Process;

typedef void (*StdOutProc) (String str);

Process* create_process(String exe, String args = "", StdOutProc stdout_proc = nullptr);
void release_process(Process *process);
bool get_exit_code(Process *process, i32 *exit_code = nullptr);

#endif // PROCESS_H
