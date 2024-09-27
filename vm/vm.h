
#if !defined(VM_HEADER_VM)
#define VM_HEADER_VM

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "../vendor/nanbox/nanbox.h"

#define VM_VERSION "0.0.5"

#define VM_NREGS 256

#define VM_GC_MIN (1 << 8)
// #define VM_GC_MIN 16
#define VM_GC_FACTOR 1.6

#define VM_DEBUG_BACKEND_BLOCKS 0
#define VM_DEBUG_BACKEND_OPCODES 0

#define VM_FORMAT_FLOAT "%.14g"

#define VM_USE_SPALL 1
#define VM_USE_SPALL_INSTR 1

#define VM_EMPTY_BYTE NANBOX_EMPTY_BYTE

struct vm_t;
struct vm_error_t;
struct vm_ir_block_t;
struct vm_ir_blocks_t;
struct vm_obj_closure_t;
struct vm_obj_gc_header_t;
struct vm_externs_t;
struct vm_obj_table_t;
struct vm_table_pair_t;
struct vm_table_pair_t;
struct vm_io_buffer_t;

typedef struct vm_io_buffer_t vm_io_buffer_t;
typedef struct vm_t vm_t;
typedef struct vm_error_t vm_error_t;
typedef struct vm_ir_block_t vm_ir_block_t;
typedef struct vm_ir_blocks_t vm_ir_blocks_t;
typedef struct vm_obj_closure_t vm_obj_closure_t;
typedef struct vm_obj_gc_header_t vm_obj_gc_header_t;
typedef struct vm_externs_t vm_externs_t;
typedef struct vm_obj_table_t vm_obj_table_t;
typedef struct vm_table_pair_t vm_table_pair_t;

typedef nanbox_t vm_obj_t;
typedef void vm_ffi_t(vm_t *vm, size_t nargs, vm_obj_t *args);

struct vm_table_pair_t {
    vm_obj_t key;
    vm_obj_t value;
};

struct vm_obj_gc_header_t {
    uint8_t mark: 1;
};

struct vm_obj_table_t {
    vm_obj_gc_header_t header;
    vm_table_pair_t *pairs;
    uint32_t len: 29;
    uint32_t used: 29;
    uint8_t size: 6;
};

struct vm_obj_closure_t {
    vm_obj_gc_header_t header;
    struct vm_ir_block_t *block;
    uint32_t len;
    vm_obj_t values[];
};

struct vm_io_buffer_t {
    vm_obj_gc_header_t header;
    char *buf;
    uint32_t len;
    uint32_t alloc;
};

struct vm_externs_t {
    size_t id;
    void *value;
    vm_externs_t *last;
};

struct vm_t {
    vm_externs_t *externs;
    vm_ir_blocks_t *blocks;

    uint8_t use_num;

    vm_obj_t std;

    void *restrict gc;

    vm_obj_t *base;
    vm_obj_t *regs;

    uint32_t nblocks;
};

vm_t *vm_state_new(void);
void vm_state_delete(vm_t *vm);

vm_obj_t vm_obj_of_string(vm_t *vm, const char *str);

#include "obj.inc"

#endif
