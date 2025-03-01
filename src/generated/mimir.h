#ifndef MIMIR_PUBLIC_H
#define MIMIR_PUBLIC_H

namespace PUBLIC {}
using namespace PUBLIC;

extern Array<LspLocation> lsp_request_definition(LspConnection *lsp, BufferId buffer_id, i32 byte_offset, Allocator mem);

#endif // MIMIR_PUBLIC_H
