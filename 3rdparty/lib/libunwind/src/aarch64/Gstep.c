/* libunwind - a platform-independent unwind library
   Copyright (C) 2008 CodeSourcery
   Copyright (C) 2011-2013 Linaro Limited
   Copyright (C) 2012 Tommi Rantala <tt.rantala@gmail.com>

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

#include "unwind_i.h"
#include "offsets.h"

PROTECTED int
unw_handle_signal_frame (unw_cursor_t *cursor)
{
  struct cursor *c = (struct cursor *) cursor;
  int ret;
  unw_word_t sc_addr, sp, sp_addr = c->dwarf.cfa;
  struct dwarf_loc sp_loc = DWARF_LOC (sp_addr, 0);

  if ((ret = dwarf_get (&c->dwarf, sp_loc, &sp)) < 0)
    return -UNW_EUNSPEC;

  ret = unw_is_signal_frame (cursor);
  Debug(1, "unw_is_signal_frame()=%d\n", ret);

  /* Save the SP and PC to be able to return execution at this point
     later in time (unw_resume).  */
  c->sigcontext_sp = c->dwarf.cfa;
  c->sigcontext_pc = c->dwarf.ip;

  if (ret)
    {
      c->sigcontext_format = AARCH64_SCF_LINUX_RT_SIGFRAME;
      sc_addr = sp_addr + sizeof (siginfo_t) + LINUX_UC_MCONTEXT_OFF;
    }
  else
    return -UNW_EUNSPEC;

  c->sigcontext_addr = sc_addr;

  /* Update the dwarf cursor.
     Set the location of the registers to the corresponding addresses of the
     uc_mcontext / sigcontext structure contents.  */
  c->dwarf.loc[UNW_AARCH64_X0]  = DWARF_LOC (sc_addr + LINUX_SC_X0_OFF, 0);
  c->dwarf.loc[UNW_AARCH64_X1]  = DWARF_LOC (sc_addr + LINUX_SC_X1_OFF, 0);
  c->dwarf.loc[UNW_AARCH64_X2]  = DWARF_LOC (sc_addr + LINUX_SC_X2_OFF, 0);
  c->dwarf.loc[UNW_AARCH64_X3]  = DWARF_LOC (sc_addr + LINUX_SC_X3_OFF, 0);
  c->dwarf.loc[UNW_AARCH64_X4]  = DWARF_LOC (sc_addr + LINUX_SC_X4_OFF, 0);
  c->dwarf.loc[UNW_AARCH64_X5]  = DWARF_LOC (sc_addr + LINUX_SC_X5_OFF, 0);
  c->dwarf.loc[UNW_AARCH64_X6]  = DWARF_LOC (sc_addr + LINUX_SC_X6_OFF, 0);
  c->dwarf.loc[UNW_AARCH64_X7]  = DWARF_LOC (sc_addr + LINUX_SC_X7_OFF, 0);
  c->dwarf.loc[UNW_AARCH64_X8]  = DWARF_LOC (sc_addr + LINUX_SC_X8_OFF, 0);
  c->dwarf.loc[UNW_AARCH64_X9]  = DWARF_LOC (sc_addr + LINUX_SC_X9_OFF, 0);
  c->dwarf.loc[UNW_AARCH64_X10] = DWARF_LOC (sc_addr + LINUX_SC_X10_OFF, 0);
  c->dwarf.loc[UNW_AARCH64_X11] = DWARF_LOC (sc_addr + LINUX_SC_X11_OFF, 0);
  c->dwarf.loc[UNW_AARCH64_X12] = DWARF_LOC (sc_addr + LINUX_SC_X12_OFF, 0);
  c->dwarf.loc[UNW_AARCH64_X13] = DWARF_LOC (sc_addr + LINUX_SC_X13_OFF, 0);
  c->dwarf.loc[UNW_AARCH64_X14] = DWARF_LOC (sc_addr + LINUX_SC_X14_OFF, 0);
  c->dwarf.loc[UNW_AARCH64_X15] = DWARF_LOC (sc_addr + LINUX_SC_X15_OFF, 0);
  c->dwarf.loc[UNW_AARCH64_X16] = DWARF_LOC (sc_addr + LINUX_SC_X16_OFF, 0);
  c->dwarf.loc[UNW_AARCH64_X17] = DWARF_LOC (sc_addr + LINUX_SC_X17_OFF, 0);
  c->dwarf.loc[UNW_AARCH64_X18] = DWARF_LOC (sc_addr + LINUX_SC_X18_OFF, 0);
  c->dwarf.loc[UNW_AARCH64_X19] = DWARF_LOC (sc_addr + LINUX_SC_X19_OFF, 0);
  c->dwarf.loc[UNW_AARCH64_X20] = DWARF_LOC (sc_addr + LINUX_SC_X20_OFF, 0);
  c->dwarf.loc[UNW_AARCH64_X21] = DWARF_LOC (sc_addr + LINUX_SC_X21_OFF, 0);
  c->dwarf.loc[UNW_AARCH64_X22] = DWARF_LOC (sc_addr + LINUX_SC_X22_OFF, 0);
  c->dwarf.loc[UNW_AARCH64_X23] = DWARF_LOC (sc_addr + LINUX_SC_X23_OFF, 0);
  c->dwarf.loc[UNW_AARCH64_X24] = DWARF_LOC (sc_addr + LINUX_SC_X24_OFF, 0);
  c->dwarf.loc[UNW_AARCH64_X25] = DWARF_LOC (sc_addr + LINUX_SC_X25_OFF, 0);
  c->dwarf.loc[UNW_AARCH64_X26] = DWARF_LOC (sc_addr + LINUX_SC_X26_OFF, 0);
  c->dwarf.loc[UNW_AARCH64_X27] = DWARF_LOC (sc_addr + LINUX_SC_X27_OFF, 0);
  c->dwarf.loc[UNW_AARCH64_X28] = DWARF_LOC (sc_addr + LINUX_SC_X28_OFF, 0);
  c->dwarf.loc[UNW_AARCH64_X29] = DWARF_LOC (sc_addr + LINUX_SC_X29_OFF, 0);
  c->dwarf.loc[UNW_AARCH64_X30] = DWARF_LOC (sc_addr + LINUX_SC_X30_OFF, 0);
  c->dwarf.loc[UNW_AARCH64_SP]  = DWARF_LOC (sc_addr + LINUX_SC_SP_OFF, 0);
  c->dwarf.loc[UNW_AARCH64_PC]  = DWARF_LOC (sc_addr + LINUX_SC_PC_OFF, 0);
  c->dwarf.loc[UNW_AARCH64_PSTATE]  = DWARF_LOC (sc_addr + LINUX_SC_PSTATE_OFF, 0);

  /* Set SP/CFA and PC/IP.  */
  dwarf_get (&c->dwarf, c->dwarf.loc[UNW_AARCH64_SP], &c->dwarf.cfa);
  dwarf_get (&c->dwarf, c->dwarf.loc[UNW_AARCH64_PC], &c->dwarf.ip);

  c->dwarf.pi_valid = 0;

  return 1;
}

PROTECTED int
unw_step (unw_cursor_t *cursor)
{
  struct cursor *c = (struct cursor *) cursor;
  int ret;

  Debug (1, "(cursor=%p, ip=0x%016lx, cfa=0x%016lx))\n",
	 c, c->dwarf.ip, c->dwarf.cfa);

  /* Check if this is a signal frame. */
  if (unw_is_signal_frame (cursor))
    return unw_handle_signal_frame (cursor);

  ret = dwarf_step (&c->dwarf);
  Debug(1, "dwarf_step()=%d\n", ret);

  if (unlikely (ret == -UNW_ESTOPUNWIND))
    return ret;

  if (unlikely (ret < 0))
    return 0;

  return (c->dwarf.ip == 0) ? 0 : 1;
}
