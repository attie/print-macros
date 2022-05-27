#!/usr/bin/env python3

import re
import argparse

class NoStateChange(Exception):
    pass

class SyncLost(Exception):
    pass

class PKDUMP:
    """
    Parse log files with the following text - produced by PKDUMP()

    ATTIE: example.c:50 example(): DUMP: user comment...
    ATTIE: example.c:50 example(): DUMP: 16 bytes @ 0x20000000
    ATTIE: example.c:50 example(): DUMP: ---8<---[ dump begins ]---8<---
    ATTIE: example.c:50 example(): DUMP: 0x0000: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 | ................
    ATTIE: example.c:50 example(): DUMP: ---8<---[  dump ends  ]---8<---
    """

    pk_handlers = (
        # current state,  next state,   regex attr,      function name
        ( 'IDLE',         'READY',     'pkdump_header', 'handle_header' ),
        ( 'READY',        'DATA',      'pkdump_begin',  'handle_begin'  ),
        ( 'DATA',         'DATA',      'pkdump_data',   'handle_data'   ),
        ( 'DATA',         'IDLE',      'pkdump_end',    'handle_end'    ),
    )

    def __init__(self, pk_tag='ATTIE'):
        self.partial = {}

        self.pkdump_line   = re.compile(b'^' + pk_tag.encode('utf-8') + b': (?P<file>.+):(?P<line>[0-9]+) (?P<func>.+)\(\): DUMP: (?P<msg>.*)$')
        self.pkdump_header = re.compile(b'^(?P<len>[0-9]+) bytes @ (?P<addr>(0x)?[0-9a-f]+)$')
        self.pkdump_begin  = re.compile(b'^---8<---\[ dump begins \]---8<---$')
        self.pkdump_data   = re.compile(b'^(?P<offset>0x[0-9a-f]+): (?P<data>(?:[0-9a-f]{2} )+) *\| .{1,16}$')
        self.pkdump_end    = re.compile(b'^---8<---\[  dump ends  \]---8<---$')

    def get_origin(self, info):
        return ( info['file'], int(info['line'], 10), info['func'] )

    def __call__(self, lineno, text):
        if (m_line := self.pkdump_line.search(text)) is None:
            return

        origin = self.get_origin(m_line)
        partial = self.partial.get(origin, {
            'state': 'IDLE',
            'section_c_file': origin[0],
            'section_c_line': origin[1],
            'section_c_func': origin[2],
        })

        valid_handlers = filter(lambda _: _[0] == partial['state'], self.pk_handlers)
        for state_cur, state_next, re_attr, fn_attr in valid_handlers:
            r = getattr(self, re_attr)
            if (m_pkdump := r.search(m_line['msg'])) is None:
                continue
            info = m_pkdump.groupdict()

            fn = getattr(self, fn_attr)
            assert(callable(fn))

            try:
                ret = fn(lineno, partial, info)
                partial['state'] = state_next
            except NoStateChange:
                continue
            except SyncLost:
                print(f'\x1b[91mWARNING: Sync lost on line {lineno}... data may be missing\x1b[0m')
                ret = None
                partial['state'] = 'IDLE'

            if partial['state'] != 'IDLE':
                self.partial[origin] = partial
            elif origin in self.partial:
                del self.partial[origin]

            return ret

    def handle_header(self, lineno, partial, info):
        data_len = int(info['len'], 10)
        if data_len == 0:
            raise NoStateChange()

        partial['section_start'] = lineno

        partial['data_len']  = data_len
        partial['data_addr'] = int(info['addr'], 16)
        partial['data_body'] = bytearray()

    def handle_begin(self, lineno, partial, info):
        pass

    def handle_data(self, lineno, partial, info):
        offset = int(info['offset'], 16)
        if len(partial['data_body']) != offset:
            raise SyncLost()

        data = [ int(_, 16) for _ in info['data'].rstrip(b' ').split(b' ') ]
        partial['data_body'].extend(data)

    def handle_end(self, lineno, partial, info):
        assert(partial['state'] == 'DATA')

        if len(partial['data_body']) != partial['data_len']:
            raise SyncLost()

        section = {
            **{ k[8:]:v for k,v in partial.items() if k[:8] == 'section_' },
            'end': lineno,
            'mode': 'PKDUMP',
        }

        data = bytes(partial['data_body'])

        return section, data

def get_args():
    parser = argparse.ArgumentParser(description="Extract BLOBs from the output of pk.h's PKDUMP() macro")
    parser.add_argument('f', metavar='filename', type=argparse.FileType('rb'), help='the log file')
    parser.add_argument('-v', '--verbose', action='store_true')
    parser.add_argument('-T', '--pk-tag',  type=str, default='ATTIE')
    parser.add_argument('--c-file',   type=str, help='only extract from this C file')
    parser.add_argument('--c-line',   type=int, help='only extract from this C line')
    parser.add_argument('--c-func',   type=str, help='only extract from this C function')
    parser.add_argument('--min-size', type=int, help='only extract BLOBs that are >= this size')
    parser.add_argument('--max-size', type=int, help='only extract BLOBs that are <= this size')
    return parser.parse_args()

def main():
    args = get_args()
    pkdump = PKDUMP(args.pk_tag)

    for lineno, text in enumerate(( _.strip(b'\r\n') for _ in args.f ), start=1):
        if args.verbose:
            print(f'{lineno:08}: {text.decode("utf-8")}')

        if (ret := pkdump(lineno, text)) is None:
            continue

        section, data = ret

        skip = None
        if args.c_file and section['c_file'].decode('utf-8') != args.c_file:
            skip = "c-file doesn't match"
        elif args.c_line and section['c_line'] != args.c_line:
            skip = "c-line doesn't match"
        elif args.c_func and section['c_func'].decode('utf-8') != args.c_func:
            skip = "c-func doesn't match"
        elif args.min_size and len(data) < args.min_size:
            skip = "too small"
        elif args.max_size and len(data) > args.max_size:
            skip = "too big"

        if skip:
            print(f'\x1b[90mSkipping   {len(data):8} bytes from lines {section["start"]:8} to {section["end"]:8} ({skip}...)\x1b[0m')
            continue

        filename = f'{section["mode"]}-line-{section["start"]:08}-to-{section["end"]:08}.bin'

        print(f'\x1b[92mExtracting {len(data):8} bytes from lines {section["start"]:8} to {section["end"]:8} into "{filename}"\x1b[0m')

        with open(filename, 'wb') as f:
            f.write(data)

if __name__ == '__main__':
    main()

