#include "../jump.h"
#include "../reguse.h"
#include "../vm.h"
#include "int.h"

#define vm_int_buf_grow()                                \
  if (buf - ret + (1 << 12) > alloc)                     \
  {                                                      \
    size_t length = buf - ret;                           \
    size_t old_alloc = alloc;                            \
    alloc = (length + 256) * 4;                          \
    ret = vm_realloc(ret, sizeof(uint8_t) * alloc);      \
    buf = ret + length;                                  \
    froms = vm_realloc(froms, sizeof(vm_loc_t) * alloc); \
    for (size_t i = old_alloc; i < alloc; i++)           \
    {                                                    \
      froms[i] = 0;                                      \
    }                                                    \
  }
#define vm_int_buf_put(type_, val_) ({*(type_*)buf = val_; buf+=sizeof(type_); })
#define vm_int_buf_put_op(op_) ({ vm_int_buf_put(void *, ptrs[op_]); })

uint8_t *vm_int_comp(size_t nops, const vm_opcode_t *ops, uint8_t *jumps, void **ptrs, vm_gc_t *restrict gc)
{
  vm_loc_t index = 0;
  vm_loc_t cfunc = 0;
  vm_loc_t cend = 0;

  vm_reg_t nregs = 16;

  size_t alloc = 1 << 8;
  uint8_t *buf = vm_malloc(sizeof(uint8_t) * alloc);
  vm_loc_t *froms = vm_alloc0(sizeof(vm_loc_t) * alloc);
  vm_loc_t *locs = vm_malloc(sizeof(vm_loc_t) * nops);
  uint8_t *ret = buf;

  uint8_t *named = vm_alloc0(sizeof(uint8_t) * (1 << 14));
  vm_value_t *regs = vm_malloc(sizeof(vm_value_t) * (1 << 12));

  while (index < nops)
  {
    // while (index < cend && (jumps[index] & VM_JUMP_REACH) == 0 && ops[index] != VM_OPCODE_FUNC)
    // {
    //   index++;
    //   while ((jumps[index] & VM_JUMP_INSTR) == 0)
    //   {
    //     index++;
    //   }
    // }
    size_t index_first = index;
    if ((jumps[index_first] & VM_JUMP_OUT) || (jumps[index_first] & VM_JUMP_IN))
    {
      for (size_t i = 0; i < nregs; i++)
      {
        if (named[i])
        {
          size_t tmpbuf[256] = {0};
          if (vm_reg_is_used(nops, ops, jumps, index, i, 255, tmpbuf, 0))
          {
            vm_int_buf_grow();
            vm_int_buf_put_op(VM_INT_OP_MOVC);
            vm_int_buf_put(vm_reg_t, i);
            vm_int_buf_put(vm_value_t, regs[i]);
          }
        }
      }
    }
    if (jumps[index_first] & VM_JUMP_IN)
    {
      for (size_t i = 0; i < nregs; i++)
      {
        named[i] = 0;
      }
    }
    vm_int_buf_grow();
    locs[index] = buf - ret;
    switch (ops[index++])
    {
    case VM_OPCODE_EXIT:
    {
      vm_int_buf_put_op(VM_INT_OP_EXIT);
      break;
    }
    case VM_OPCODE_REG:
    {
      vm_opcode_t out = ops[index++];
      vm_opcode_t in = ops[index++];
      if (named[in])
      {
        named[out] = 1;
        regs[out] = regs[in];
      }
      else
      {
        named[out] = 0;
        vm_int_buf_put_op(VM_INT_OP_MOV);
        vm_int_buf_put(vm_reg_t, out);
        vm_int_buf_put(vm_reg_t, in);
      }
      break;
    }
    case VM_OPCODE_INT:
    {
      vm_opcode_t out = ops[index++];
      vm_opcode_t in = ops[index++];
      named[out] = 1;
      regs[out] = vm_value_from_int(gc, in);
      break;
    }
    case VM_OPCODE_NEG:
    {
      vm_opcode_t out = ops[index++];
      vm_opcode_t in = ops[index++];
      named[out] = 1;
      regs[out] = vm_value_from_int(gc, -(vm_int_t)in);
      break;
    }
    case VM_OPCODE_JUMP:
    {
      vm_opcode_t loc = ops[index++];
      vm_int_buf_put_op(VM_INT_OP_JUMP);
      froms[buf - ret] = loc;
      vm_int_buf_put(vm_loc_t, 0);
      break;
    }
    case VM_OPCODE_DJUMP:
    {
      vm_opcode_t locreg = ops[index++];
      vm_int_buf_put_op(VM_INT_OP_DJUMP);
      vm_int_buf_put(vm_reg_t, locreg);
      break;
    }
    case VM_OPCODE_FUNC:
    {
      for (size_t i = 0; i < nregs; i++)
      {
        named[i] = 0;
      }
      cend = ops[index++];
      vm_opcode_t nargs = ops[index++];
      nregs = ops[index++];
      vm_int_buf_put_op(VM_INT_OP_JUMP);
      froms[buf - ret] = cend;
      vm_int_buf_put(vm_loc_t, 0);
      vm_int_buf_put(vm_reg_t, nregs);
      cfunc = index;
      break;
    }
    case VM_OPCODE_ADD:
    {
      vm_opcode_t out = ops[index++];
      vm_opcode_t lhs = ops[index++];
      vm_opcode_t rhs = ops[index++];
      if (named[lhs] && named[rhs])
      {
        named[out] = 1;
        regs[out] = vm_value_add(gc, regs[lhs], regs[rhs]);
      }
      else
      {
        if (named[lhs])
        {
          if (VM_VALUE_IS_INT32(regs[lhs]))
          {
            vm_int_buf_put_op(VM_INT_OP_ADDI);
            vm_int_buf_put(vm_reg_t, out);
            vm_int_buf_put(vm_reg_t, rhs);
            vm_int_buf_put(vm_int_t, VM_VALUE_GET_INT32(regs[lhs]));
          }
          else
          {
            vm_int_buf_put_op(VM_INT_OP_ADDC);
            vm_int_buf_put(vm_reg_t, out);
            vm_int_buf_put(vm_reg_t, rhs);
            vm_int_buf_put(vm_value_t, regs[lhs]);
          }
        }
        else if (named[rhs])
        {
          if (VM_VALUE_IS_INT32(regs[rhs]))
          {
            vm_int_buf_put_op(VM_INT_OP_ADDC);
            vm_int_buf_put(vm_reg_t, out);
            vm_int_buf_put(vm_reg_t, lhs);
            vm_int_buf_put(vm_value_t, regs[rhs]);
          }
          else
          {
            vm_int_buf_put_op(VM_INT_OP_ADDI);
            vm_int_buf_put(vm_reg_t, out);
            vm_int_buf_put(vm_reg_t, lhs);
            vm_int_buf_put(vm_int_t, VM_VALUE_GET_INT32(regs[rhs]));
          }
        }
        else
        {
          vm_int_buf_put_op(VM_INT_OP_ADD);
          vm_int_buf_put(vm_reg_t, out);
          vm_int_buf_put(vm_reg_t, lhs);
          vm_int_buf_put(vm_reg_t, rhs);
        }
        named[out] = 0;
      }
      break;
    }
    case VM_OPCODE_SUB:
    {
      vm_opcode_t out = ops[index++];
      vm_opcode_t lhs = ops[index++];
      vm_opcode_t rhs = ops[index++];
      if (named[lhs] && named[rhs])
      {
        named[out] = 1;
        regs[out] = vm_value_sub(gc, regs[lhs], regs[rhs]);
      }
      else
      {
        if (named[lhs])
        {
          if (VM_VALUE_IS_INT32(regs[lhs]))
          {
            vm_int_buf_put_op(VM_INT_OP_ISUB);
            vm_int_buf_put(vm_reg_t, out);
            vm_int_buf_put(vm_int_t, VM_VALUE_GET_INT32(regs[lhs]));
            vm_int_buf_put(vm_reg_t, rhs);
          }
          else
          {
            vm_int_buf_put_op(VM_INT_OP_CSUB);
            vm_int_buf_put(vm_reg_t, out);
            vm_int_buf_put(vm_value_t, regs[lhs]);
            vm_int_buf_put(vm_reg_t, rhs);
          }
        }
        else if (named[rhs])
        {
          if (VM_VALUE_IS_INT32(regs[rhs]))
          {
            vm_int_buf_put_op(VM_INT_OP_SUBI);
            vm_int_buf_put(vm_reg_t, out);
            vm_int_buf_put(vm_reg_t, lhs);
            vm_int_buf_put(vm_int_t, VM_VALUE_GET_INT32(regs[rhs]));
          }
          else
          {
            vm_int_buf_put_op(VM_INT_OP_SUBC);
            vm_int_buf_put(vm_reg_t, out);
            vm_int_buf_put(vm_reg_t, lhs);
            vm_int_buf_put(vm_value_t, regs[rhs]);
          }
        }
        else
        {
          vm_int_buf_put_op(VM_INT_OP_SUB);
          vm_int_buf_put(vm_reg_t, out);
          vm_int_buf_put(vm_reg_t, lhs);
          vm_int_buf_put(vm_reg_t, rhs);
        }
        named[out] = 0;
      }
      break;
    }
    case VM_OPCODE_CALL:
    {
      vm_opcode_t rreg = ops[index++];
      vm_opcode_t func = ops[index++];
      vm_opcode_t nargs = ops[index++];
      for (int i = 0; i < nargs; i++)
      {
        vm_opcode_t reg = ops[index + i];
        if (named[reg])
        {
          vm_int_buf_put_op(VM_INT_OP_MOVC);
          vm_int_buf_put(vm_reg_t, reg);
          vm_int_buf_put(vm_value_t, regs[reg]);
        }
      }
      named[rreg] = 0;
      vm_int_buf_put_op(VM_INT_OP_CALL0 + nargs);
      froms[buf - ret] = func;
      vm_int_buf_put(vm_loc_t, 0);
      for (int i = 0; i < nargs; i++)
      {
        vm_int_buf_put(vm_reg_t, ops[index++]);
      }
      vm_int_buf_put(vm_reg_t, nregs);
      vm_int_buf_put(vm_reg_t, rreg);
      break;
    }
    case VM_OPCODE_DCALL:
    {
      vm_opcode_t rreg = ops[index++];
      vm_opcode_t func = ops[index++];
      vm_opcode_t nargs = ops[index++];
      for (int i = 0; i < nargs; i++)
      {
        vm_opcode_t reg = ops[index + i];
        if (named[reg])
        {
          vm_int_buf_put_op(VM_INT_OP_MOVC);
          vm_int_buf_put(vm_reg_t, reg);
          vm_int_buf_put(vm_value_t, regs[reg]);
        }
      }
      named[rreg] = 0;
      vm_int_buf_put_op(VM_INT_OP_DCALL0 + nargs);
      vm_int_buf_put(vm_reg_t, func);
      for (int i = 0; i < nargs; i++)
      {
        vm_int_buf_put(vm_reg_t, ops[index++]);
      }
      vm_int_buf_put(vm_reg_t, nregs);
      vm_int_buf_put(vm_reg_t, rreg);
      break;
    }
    case VM_OPCODE_RET:
    {
      vm_opcode_t out = ops[index++];
      if (named[out])
      {
        vm_int_buf_put_op(VM_INT_OP_RETC);
        vm_int_buf_put(vm_value_t, regs[out]);
      }
      else
      {
        vm_int_buf_put_op(VM_INT_OP_RET);
        vm_int_buf_put(vm_reg_t, out);
      }
      break;
    }
    case VM_OPCODE_PUTCHAR:
    {
      vm_opcode_t in = ops[index++];
      if (named[in])
      {
        vm_int_buf_put_op(VM_INT_OP_MOVC);
        vm_int_buf_put(vm_reg_t, in);
        vm_int_buf_put(vm_value_t, regs[in]);
      }
      vm_int_buf_put_op(VM_INT_OP_PUTC);
      vm_int_buf_put(vm_reg_t, in);
      break;
    }
    case VM_OPCODE_BEQ:
    {
      vm_opcode_t lhs = ops[index++];
      vm_opcode_t rhs = ops[index++];
      vm_opcode_t jfalse = ops[index++];
      vm_opcode_t jtrue = ops[index++];
      if (named[lhs] && named[rhs])
      {
        if (vm_value_is_equal(gc, regs[lhs], regs[rhs]))
        {
          vm_int_buf_put_op(VM_INT_OP_JUMP);
          froms[buf - ret] = jtrue;
          vm_int_buf_put(vm_loc_t, 0);
        }
        else
        {
          vm_int_buf_put_op(VM_INT_OP_JUMP);
          froms[buf - ret] = jfalse;
          vm_int_buf_put(vm_loc_t, 0);
        }
      }
      else if (named[rhs])
      {
        if (VM_VALUE_IS_INT32(regs[rhs])) {
          vm_int_buf_put_op(VM_INT_OP_BEQI);
          vm_int_buf_put(vm_reg_t, lhs);
          vm_int_buf_put(vm_int_t, VM_VALUE_GET_INT32(regs[rhs]));
          froms[buf - ret] = jfalse;
          vm_int_buf_put(vm_loc_t, 0);
          froms[buf - ret] = jtrue;
          vm_int_buf_put(vm_loc_t, 0);
        } else {
          vm_int_buf_put_op(VM_INT_OP_BEQC);
          vm_int_buf_put(vm_reg_t, lhs);
          vm_int_buf_put(vm_value_t, regs[rhs]);
          froms[buf - ret] = jfalse;
          vm_int_buf_put(vm_loc_t, 0);
          froms[buf - ret] = jtrue;
          vm_int_buf_put(vm_loc_t, 0);
        }
      }
      else if (named[lhs])
      {
        if (VM_VALUE_IS_INT32(regs[lhs])) {
          vm_int_buf_put_op(VM_INT_OP_BEQI);
          vm_int_buf_put(vm_reg_t, lhs);
          vm_int_buf_put(vm_int_t, VM_VALUE_GET_INT32(regs[lhs]));
          froms[buf - ret] = jfalse;
          vm_int_buf_put(vm_loc_t, 0);
          froms[buf - ret] = jtrue;
          vm_int_buf_put(vm_loc_t, 0);
        } else {
          vm_int_buf_put_op(VM_INT_OP_BEQC);
          vm_int_buf_put(vm_reg_t, rhs);
          vm_int_buf_put(vm_value_t, regs[lhs]);
          froms[buf - ret] = jfalse;
          vm_int_buf_put(vm_loc_t, 0);
          froms[buf - ret] = jtrue;
          vm_int_buf_put(vm_loc_t, 0);
        }
      }
      else
      {
        vm_int_buf_put_op(VM_INT_OP_BEQ);
        vm_int_buf_put(vm_reg_t, lhs);
        vm_int_buf_put(vm_reg_t, rhs);
        froms[buf - ret] = jfalse;
        vm_int_buf_put(vm_loc_t, 0);
        froms[buf - ret] = jtrue;
        vm_int_buf_put(vm_loc_t, 0);
      }
      break;
    }
    case VM_OPCODE_ADDR:
    {
      vm_opcode_t out = ops[index++];
      vm_opcode_t func = ops[index++];
      vm_int_buf_put_op(VM_INT_OP_MOVF);
      vm_int_buf_put(vm_reg_t, out);
      froms[buf - ret] = func;
      vm_int_buf_put(vm_loc_t, 0);
      named[out] = 0;
      break;
    }
    case VM_OPCODE_MUL:
    {
      vm_opcode_t out = ops[index++];
      vm_opcode_t lhs = ops[index++];
      vm_opcode_t rhs = ops[index++];
      if (named[lhs] && named[rhs])
      {
        named[out] = 1;
        regs[out] = vm_value_mul(gc, regs[lhs], regs[rhs]);
      }
      else
      {
        if (named[lhs])
        {
          if (VM_VALUE_IS_INT32(regs[lhs]))
          {
            vm_int_buf_put_op(VM_INT_OP_MULI);
            vm_int_buf_put(vm_reg_t, out);
            vm_int_buf_put(vm_reg_t, rhs);
            vm_int_buf_put(vm_int_t, VM_VALUE_GET_INT32(regs[lhs]));
          }
          else
          {
            vm_int_buf_put_op(VM_INT_OP_MULC);
            vm_int_buf_put(vm_reg_t, out);
            vm_int_buf_put(vm_reg_t, rhs);
            vm_int_buf_put(vm_value_t, regs[lhs]);
          }
        }
        else if (named[rhs])
        {
          if (VM_VALUE_IS_INT32(regs[rhs]))
          {
            vm_int_buf_put_op(VM_INT_OP_MULI);
            vm_int_buf_put(vm_reg_t, out);
            vm_int_buf_put(vm_reg_t, lhs);
            vm_int_buf_put(vm_value_t, regs[rhs]);
          }
          else
          {
            vm_int_buf_put_op(VM_INT_OP_MULC);
            vm_int_buf_put(vm_reg_t, out);
            vm_int_buf_put(vm_reg_t, lhs);
            vm_int_buf_put(vm_int_t, VM_VALUE_GET_INT32(regs[rhs]));
          }
        }
        else
        {
          vm_int_buf_put_op(VM_INT_OP_MUL);
          vm_int_buf_put(vm_reg_t, out);
          vm_int_buf_put(vm_reg_t, lhs);
          vm_int_buf_put(vm_reg_t, rhs);
        }
        named[out] = 0;
      }
      break;
    }
    case VM_OPCODE_DIV:
    {
      vm_opcode_t out = ops[index++];
      vm_opcode_t lhs = ops[index++];
      vm_opcode_t rhs = ops[index++];
      if (named[lhs] && named[rhs])
      {
        named[out] = 1;
        regs[out] = vm_value_div(gc, regs[lhs], regs[rhs]);
      }
      else
      {
        if (named[lhs])
        {
          if (VM_VALUE_IS_INT32(regs[lhs]))
          {
            vm_int_buf_put_op(VM_INT_OP_IDIV);
            vm_int_buf_put(vm_reg_t, out);
            vm_int_buf_put(vm_int_t, VM_VALUE_GET_INT32(regs[lhs]));
            vm_int_buf_put(vm_reg_t, rhs);
          }
          else
          {
            vm_int_buf_put_op(VM_INT_OP_CDIV);
            vm_int_buf_put(vm_reg_t, out);
            vm_int_buf_put(vm_value_t, regs[lhs]);
            vm_int_buf_put(vm_reg_t, rhs);
          }
        }
        else if (named[rhs])
        {
          if (VM_VALUE_IS_INT32(regs[rhs]))
          {
            vm_int_buf_put_op(VM_INT_OP_DIVI);
            vm_int_buf_put(vm_reg_t, out);
            vm_int_buf_put(vm_reg_t, lhs);
            vm_int_buf_put(vm_int_t, VM_VALUE_GET_INT32(regs[rhs]));
          }
          else
          {
            vm_int_buf_put_op(VM_INT_OP_DIVC);
            vm_int_buf_put(vm_reg_t, out);
            vm_int_buf_put(vm_reg_t, lhs);
            vm_int_buf_put(vm_value_t, regs[rhs]);
          }
        }
        else
        {
          vm_int_buf_put_op(VM_INT_OP_DIV);
          vm_int_buf_put(vm_reg_t, out);
          vm_int_buf_put(vm_reg_t, lhs);
          vm_int_buf_put(vm_reg_t, rhs);
        }
        named[out] = 0;
      }
      break;
    }
    case VM_OPCODE_MOD:
    {
      vm_opcode_t out = ops[index++];
      vm_opcode_t lhs = ops[index++];
      vm_opcode_t rhs = ops[index++];
      if (named[lhs] && named[rhs])
      {
        named[out] = 1;
        regs[out] = vm_value_mod(gc, regs[lhs], regs[rhs]);
      }
      else
      {
        if (named[lhs])
        {
          if (VM_VALUE_IS_INT32(regs[lhs]))
          {
            vm_int_buf_put_op(VM_INT_OP_IMOD);
            vm_int_buf_put(vm_reg_t, out);
            vm_int_buf_put(vm_int_t, VM_VALUE_GET_INT32(regs[lhs]));
            vm_int_buf_put(vm_reg_t, rhs);
          }
          else
          {
            vm_int_buf_put_op(VM_INT_OP_CMOD);
            vm_int_buf_put(vm_reg_t, out);
            vm_int_buf_put(vm_value_t, regs[lhs]);
            vm_int_buf_put(vm_reg_t, rhs);
          }
        }
        else if (named[rhs])
        {
          if (VM_VALUE_IS_INT32(regs[rhs]))
          {
            vm_int_buf_put_op(VM_INT_OP_MODI);
            vm_int_buf_put(vm_reg_t, out);
            vm_int_buf_put(vm_reg_t, lhs);
            vm_int_buf_put(vm_int_t, VM_VALUE_GET_INT32(regs[rhs]));
          }
          else
          {
            vm_int_buf_put_op(VM_INT_OP_MODC);
            vm_int_buf_put(vm_reg_t, out);
            vm_int_buf_put(vm_reg_t, lhs);
            vm_int_buf_put(vm_value_t, regs[rhs]);
          }
        }
        else
        {
          vm_int_buf_put_op(VM_INT_OP_MOD);
          vm_int_buf_put(vm_reg_t, out);
          vm_int_buf_put(vm_reg_t, lhs);
          vm_int_buf_put(vm_reg_t, rhs);
        }
        named[out] = 0;
      }
      break;
    }
    case VM_OPCODE_BB:
    {
      vm_opcode_t val = ops[index++];
      vm_opcode_t jfalse = ops[index++];
      vm_opcode_t jtrue = ops[index++];
      if (named[val])
      {
        if (vm_value_to_bool(gc, regs[val]))
        {
          vm_int_buf_put_op(VM_INT_OP_JUMP);
          froms[buf - ret] = jtrue;
          vm_int_buf_put(vm_loc_t, 0);
        }
        else
        {
          vm_int_buf_put_op(VM_INT_OP_JUMP);
          froms[buf - ret] = jfalse;
          vm_int_buf_put(vm_loc_t, 0);
        }
      }
      else
      {
        vm_int_buf_put_op(VM_INT_OP_BB);
        vm_int_buf_put(vm_reg_t, val);
        froms[buf - ret] = jfalse;
        vm_int_buf_put(vm_loc_t, 0);
        froms[buf - ret] = jtrue;
        vm_int_buf_put(vm_loc_t, 0);
      }
      break;
    }
    case VM_OPCODE_BLT:
    {
      vm_opcode_t lhs = ops[index++];
      vm_opcode_t rhs = ops[index++];
      vm_opcode_t jfalse = ops[index++];
      vm_opcode_t jtrue = ops[index++];
      if (named[lhs] && named[rhs])
      {
        if (vm_value_is_less(gc, regs[lhs], regs[rhs]))
        {
          vm_int_buf_put_op(VM_INT_OP_JUMP);
          froms[buf - ret] = jtrue;
          vm_int_buf_put(vm_loc_t, 0);
        }
        else
        {
          vm_int_buf_put_op(VM_INT_OP_JUMP);
          froms[buf - ret] = jfalse;
          vm_int_buf_put(vm_loc_t, 0);
        }
      }
      else if (named[rhs])
      {
        if (VM_VALUE_IS_INT32(regs[rhs])) {
          vm_int_buf_put_op(VM_INT_OP_BLTI);
          vm_int_buf_put(vm_reg_t, lhs);
          vm_int_buf_put(vm_int_t, VM_VALUE_GET_INT32(regs[rhs]));
          froms[buf - ret] = jfalse;
          vm_int_buf_put(vm_loc_t, 0);
          froms[buf - ret] = jtrue;
          vm_int_buf_put(vm_loc_t, 0);
        } else {
          vm_int_buf_put_op(VM_INT_OP_BLTC);
          vm_int_buf_put(vm_reg_t, lhs);
          vm_int_buf_put(vm_value_t, regs[rhs]);
          froms[buf - ret] = jfalse;
          vm_int_buf_put(vm_loc_t, 0);
          froms[buf - ret] = jtrue;
          vm_int_buf_put(vm_loc_t, 0);
        }
      }
      else if (named[lhs])
      {
        if (VM_VALUE_IS_INT32(regs[lhs])) {
          vm_int_buf_put_op(VM_INT_OP_IBLT);
          vm_int_buf_put(vm_int_t, VM_VALUE_GET_INT32(regs[lhs]));
          vm_int_buf_put(vm_reg_t, rhs);
          froms[buf - ret] = jfalse;
          vm_int_buf_put(vm_loc_t, 0);
          froms[buf - ret] = jtrue;
          vm_int_buf_put(vm_loc_t, 0);
        } else {
          vm_int_buf_put_op(VM_INT_OP_CBLT);
          vm_int_buf_put(vm_value_t, regs[lhs]);
          vm_int_buf_put(vm_reg_t, rhs);
          froms[buf - ret] = jfalse;
          vm_int_buf_put(vm_loc_t, 0);
          froms[buf - ret] = jtrue;
          vm_int_buf_put(vm_loc_t, 0);
        }
      }
      else
      {
        vm_int_buf_put_op(VM_INT_OP_BLT);
        vm_int_buf_put(vm_reg_t, lhs);
        vm_int_buf_put(vm_reg_t, rhs);
        froms[buf - ret] = jfalse;
        vm_int_buf_put(vm_loc_t, 0);
        froms[buf - ret] = jtrue;
        vm_int_buf_put(vm_loc_t, 0);
      }
      break;
    }
    default:
    {
      vm_free(ret);
      vm_free(regs);
      vm_free(named);
      vm_free(froms);
      vm_free(locs);
      return NULL;
    }
    }
    if (jumps[index_first] & VM_JUMP_OUT)
    {
      for (size_t i = 0; i < nregs; i++)
      {
        named[i] = 0;
      }
    }
  }
  size_t length = buf - ret;
  for (size_t i = 0; i < length; i++)
  {
    if (froms[i] != 0)
    {
      *(vm_loc_t *)&ret[i] = locs[froms[i]];
    }
  }
  vm_free(regs);
  vm_free(named);
  vm_free(froms);
  vm_free(locs);
  return ret;
}
