#!/usr/bin/env python3
# Copyright Microsoft and CHERIoT Contributors.
# SPDX-License-Identifier: MIT

import optparse, re, bisect, sys, subprocess

nm_re=re.compile('([0-9a-f]+) . (.+)')
trace_re=re.compile('(?P<pc>[0-9a-fA-F]{8})')

def usage(msg):
    sys.stderr.write(msg + "\n")
    sys.exit(1)

def get_names(options):
    if options.exe_file:
        p = subprocess.Popen(['nm','--demangle', options.exe_file],stdout=subprocess.PIPE,text=True)
        nm_file = p.stdout
    elif options.nm_file:
        nm_file = open(options.nm_file, 'r')
    else:
        usage("Error: please provide either exe file or nm output.")
    print("Parsing nm file...")
    names=[]
    for line in nm_file:
        # line=str(line)
        m=nm_re.match(line)
        if not m:
            sys.stderr.write(f"Warning: unrecognized line in nm file: {line}")
            continue
        names.append((int(m.group(1), 16), m.group(2)))
    # Sort by address
    names.sort(key=lambda x: x[0])
    # Got the data, now merge duplicate symbols into one.
    print("Removing duplicate symbols...")
    unique_names=[]
    prev_addr, prev_name=0, 'null'
    for (addr, name) in names:
        if addr != prev_addr:
            unique_names.append((prev_addr, prev_name))
            prev_name = name
            prev_addr = addr
        else:
            prev_name = prev_name + '/' + name
    unique_names.append((prev_addr, prev_name))
    print("Loaded names.")
    return unique_names

def annotate_trace(options):
    names = get_names(options)
    non_local_names = filter(lambda x: not x[1].startswith('.'), names)
    (addresses, syms) = list(zip(*names))
    (non_local_addresses, non_local_syms) = list(zip(*non_local_names))
    if options.trace_file is None:
        trace_file=sys.stdin
    else:
        trace_file=open(options.trace_file, 'r')
    for line in trace_file:
        m = trace_re.search(line)
        if m:
            addr=int(m.group('pc'), 16)
            i = bisect.bisect_right(addresses, addr)-1
            sym = syms[i]
            i2 = bisect.bisect_right(non_local_addresses, addr) - 1
            non_local_sym = non_local_syms[i2]
            if non_local_sym != sym:
                sym = f"{non_local_sym} / {sym}"
            print(f"{line[:-1]:{options.width}s} | {sym}")
        else:
            print(line[:-1])

if __name__=="__main__":
    parser = optparse.OptionParser("""%prog --exe <EXE> --trace <TRACE>
    Simple script to annotate an instruction trace with function names.
    Assumes the first 8 digit hex number in each input line is the program counter.""")
    parser.add_option('-e','--exe', dest="exe_file", help="Elf file containing symbol names")
    parser.add_option('-N','--nm', dest="nm_file", help="File containing output of nm (alternative to elf file)")
    parser.add_option('-t','--trace', dest="trace_file", help="Trace file to annotate (default stdin)", default=None)
    parser.add_option('-w','--width', help="Width to pad input lines to before annotating", type=int, default=70)
    (opts, args) = parser.parse_args()
    annotate_trace(opts)
