#!/usr/bin/env python
#
# Copyright (C) 2010-2011 Alex Smith
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

import sys

if len(sys.argv) != 4:
    print('Usage: %s <input file> <table name> <size name>' % (sys.argv[0]))
    sys.exit(1)

# Read in the input file and generate a sorted list of errors.
f = open(sys.argv[1], 'r')
errors = []
for s in f.readlines():
    line = s.strip().split()
    if len(line) < 6 or line[0] != '#define' or not line[2].isdigit():
        continue
    errors.append((int(line[2]), ' '.join(line[4:-1])[0:-1]))
errors.sort(key=lambda x: x[0])

# Generate the output.
print('/* This file is automatically generated. Do not edit! */')
print('#include <stddef.h>')
print('const char *%s[] __attribute__((visibility("default"))) = {' % (sys.argv[2]))
for err in errors:
    print(' [%d] = "%s",' % (err[0], err[1]))
print('};')
print('size_t %s __attribute__((visibility("default"))) = %d;' % (sys.argv[3], errors[-1][0] + 1))