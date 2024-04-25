#include "process.h"
#include "core/core.h"

Process* create_process(String /*exe*/, String /*args*/, StdOutProc /*stdout_proc*/)
{
	LOG_ERROR("unimplemented");
	return nullptr;
}

void release_process(Process * /*process*/)
{
	LOG_ERROR("unimplemented");
}

bool get_exit_code(Process * /*process*/, i32 * /*exit_code*/)
{
	LOG_ERROR("unimplemented");
	return false;
}
