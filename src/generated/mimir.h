#ifndef MIMIR_PUBLIC_H
#define MIMIR_PUBLIC_H

namespace PUBLIC {}
using namespace PUBLIC;

extern bool json_parse(LspLocation *result, String json, Allocator mem);
extern bool json_parse(LspTextDocumentSyncOptions *result, String json, Allocator mem);
extern Array<LspLocation> lsp_request_definition(LspConnection *lsp, BufferId buffer_id, i32 byte_offset, Allocator mem);

#endif // MIMIR_PUBLIC_H
