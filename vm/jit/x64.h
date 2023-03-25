#if !defined(VM_HEADER_BE_X64)
#define VM_HEADER_BE_X64

#include "../lib.h"
#include "../ir.h"
#include "../type.h"

struct vm_x64_func_buf_t;
typedef struct vm_x64_func_buf_t vm_x64_func_buf_t;

struct vm_x64_mmap_t;
typedef struct vm_x64_mmap_t vm_x64_mmap_t;

struct vm_x64_map_buf_t;
typedef struct vm_x64_map_buf_t vm_x64_map_buf_t;

struct vm_x64_state_t;
typedef struct vm_x64_state_t vm_x64_state_t;

struct vm_x64_cache_t;
typedef struct vm_x64_cache_t vm_x64_cache_t;

struct vm_x64_func_buf_t {
    void **funcs;
    size_t alloc;
    size_t len;
};

struct vm_x64_mmap_t {
    void *mem;
    size_t alloc;
    size_t used;
};

struct vm_x64_map_buf_t {
    vm_x64_mmap_t *mmaps;
    size_t alloc;
    size_t len;
};

struct vm_x64_state_t {
    void *exitptr;
    size_t count;
    vm_x64_func_buf_t funcbuf;
    vm_x64_map_buf_t mapbuf;
    size_t push;
};

struct vm_x64_cache_t {
    uint16_t args[256];
    vm_block_t *block;
};

vm_x64_cache_t *vm_x64_cache_new(void);
vm_block_t *vm_x64_rblock_version(vm_rblock_t *rblock);
void *vm_x64_func_comp(vm_x64_state_t *state, vm_block_t *block);
void *vm_x64_full_comp(vm_x64_state_t *state, vm_block_t *block);
void vm_x64_run(vm_block_t *block);

#endif