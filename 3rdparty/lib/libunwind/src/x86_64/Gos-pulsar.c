/* libunwind - a platform-independent unwind library
   Copyright (C) 2013 Alex Smith <alex@alex-smith.me.uk>

This file is part of libunwind.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.  */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <signal.h>
#include <stdio.h>

#include "unwind_i.h"
#include "ucontext_i.h"

PROTECTED int
unw_is_signal_frame (unw_cursor_t *cursor)
{
  struct cursor *c = (struct cursor *) cursor;

  c->sigcontext_format = X86_64_SCF_NONE;
  return (X86_64_SCF_NONE);
}

PROTECTED int
unw_handle_signal_frame (unw_cursor_t *cursor)
{
  return -UNW_EBADFRAME;
}

#ifndef UNW_REMOTE_ONLY
HIDDEN void *
x86_64_r_uc_addr (ucontext_t *uc, int reg)
{
  /* NOTE: common_init() in init.h inlines these for fast path access. */
  void *addr;

  switch (reg)
    {
    case UNW_X86_64_R8: addr = &uc->uc_mcontext.r8; break;
    case UNW_X86_64_R9: addr = &uc->uc_mcontext.r9; break;
    case UNW_X86_64_R10: addr = &uc->uc_mcontext.r10; break;
    case UNW_X86_64_R11: addr = &uc->uc_mcontext.r11; break;
    case UNW_X86_64_R12: addr = &uc->uc_mcontext.r12; break;
    case UNW_X86_64_R13: addr = &uc->uc_mcontext.r13; break;
    case UNW_X86_64_R14: addr = &uc->uc_mcontext.r14; break;
    case UNW_X86_64_R15: addr = &uc->uc_mcontext.r15; break;
    case UNW_X86_64_RDI: addr = &uc->uc_mcontext.rdi; break;
    case UNW_X86_64_RSI: addr = &uc->uc_mcontext.rsi; break;
    case UNW_X86_64_RBP: addr = &uc->uc_mcontext.rbp; break;
    case UNW_X86_64_RBX: addr = &uc->uc_mcontext.rbx; break;
    case UNW_X86_64_RDX: addr = &uc->uc_mcontext.rdx; break;
    case UNW_X86_64_RAX: addr = &uc->uc_mcontext.rax; break;
    case UNW_X86_64_RCX: addr = &uc->uc_mcontext.rcx; break;
    case UNW_X86_64_RSP: addr = &uc->uc_mcontext.rsp; break;
    case UNW_X86_64_RIP: addr = &uc->uc_mcontext.rip; break;

    default:
      addr = NULL;
    }
  return addr;
}

HIDDEN NORETURN void
x86_64_sigreturn (unw_cursor_t *cursor)
{
  fprintf(stderr, "called x86_64_sigreturn\n");
  abort();
}
#endif
