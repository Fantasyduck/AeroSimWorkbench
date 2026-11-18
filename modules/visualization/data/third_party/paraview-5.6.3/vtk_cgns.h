#pragma once
#include <cgnslib.h>
#include <cgns_io.h>

// ParaView 5.6 used CGIO entry points removed in CGNS 4.1.  Their typed
// replacements preserve the original behavior by requesting the node's stored
// data type. Kept local to this experimental reader; no global symbol shim.
#if CGNS_VERSION >= 4100
inline int cgio_read_all_data(int file, double id, void *data)
{
    char type[3] = {};
    const int status = cgio_get_data_type(file, id, type);
    return status ? status : cgio_read_all_data_type(file, id, type, data);
}
inline int cgio_read_block_data(int file, double id, cgsize_t first, cgsize_t last, void *data)
{
    char type[3] = {};
    const int status = cgio_get_data_type(file, id, type);
    return status ? status : cgio_read_block_data_type(file, id, first, last, type, data);
}
inline int cgio_read_data(int file, double id, const cgsize_t *start, const cgsize_t *end,
                          const cgsize_t *stride, int dims, const cgsize_t *sizes,
                          const cgsize_t *memStart, const cgsize_t *memEnd,
                          const cgsize_t *memStride, void *data)
{
    char type[3] = {};
    const int status = cgio_get_data_type(file, id, type);
    return status ? status : cgio_read_data_type(file, id, start, end, stride, type,
        dims, sizes, memStart, memEnd, memStride, data);
}
#endif
