#!/usr/bin/env python3
"""Minimal reader for cooked UE 4.19 packages (.uasset + .uexp), enough to
walk tagged properties of widget blueprints and report file offsets of values."""
import struct, sys, os

NATIVE_STRUCTS = {
    'Vector2D': 'ff', 'Vector': 'fff', 'Rotator': 'fff', 'Quat': 'ffff',
    'LinearColor': 'ffff', 'Color': 'BBBB', 'IntPoint': 'ii', 'Guid': '16s',
    'Vector4': 'ffff', 'Box2D': 'ffffB', 'IntVector': 'iii',
}


class R:
    def __init__(self, b, pos=0):
        self.b, self.p = b, pos

    def u(self, fmt):
        v = struct.unpack_from('<' + fmt, self.b, self.p)
        self.p += struct.calcsize('<' + fmt)
        return v if len(v) > 1 else v[0]

    def fstr(self):
        n = self.u('i')
        if n == 0:
            return ''
        if n < 0:
            s = self.b[self.p:self.p - n * 2 - 2].decode('utf-16-le')
            self.p += -n * 2
            return s
        s = self.b[self.p:self.p + n - 1].decode('latin1')
        self.p += n
        return s


class Package:
    def __init__(self, path):
        base = path[:-7] if path.endswith('.uasset') else path
        self.base = base
        self.ua = bytearray(open(base + '.uasset', 'rb').read())
        self.ue = bytearray(open(base + '.uexp', 'rb').read()) if os.path.exists(base + '.uexp') else bytearray()
        self.parse()

    def parse(self):
        r = R(self.ua)
        assert r.u('I') == 0x9E2A83C1
        legacy = r.u('i'); r.u('i'); r.u('i'); r.u('i')
        ncv = r.u('i'); r.p += ncv * 20
        self.header_size = r.u('i')
        r.fstr(); r.u('I')
        self.name_count, self.name_off = r.u('ii')
        r.u('ii')
        self.export_count, self.export_off, self.import_count, self.import_off, self.depends_off = r.u('iiiii')
        r = R(self.ua, self.name_off)
        self.names = []
        for _ in range(self.name_count):
            self.names.append(r.fstr()); r.u('HH')
        r = R(self.ua, self.import_off)
        self.imports = []
        for _ in range(self.import_count):
            cp, cn, outer, on = self.fname(r), self.fname(r), r.u('i'), self.fname(r)
            self.imports.append(dict(cls=cn, outer=outer, name=on, pkg=cp))
        r = R(self.ua, self.export_off)
        self.exports = []
        for i in range(self.export_count):
            entry_pos = r.p
            ci, si, ti, oi = r.u('iiii')
            name = self.fname(r)
            flags = r.u('I')
            ssize, soff = r.u('qq')
            r.p += 12 + 16 + 4 + 4 + 4 + 20
            self.exports.append(dict(i=i, cls=ci, sup=si, tmpl=ti, outer=oi, name=name, size=ssize, off=soff,
                                     entry_pos=entry_pos))

    def fname(self, r):
        idx, num = r.u('ii')
        n = self.names[idx]
        return n if num == 0 else f'{n}_{num - 1}'

    def ref(self, idx):
        if idx == 0:
            return 'null'
        if idx < 0:
            return 'imp:' + self.imports[-idx - 1]['name']
        return f'exp{idx - 1}:' + self.exports[idx - 1]['name']

    def cls(self, e):
        return self.ref(e['cls']).split(':', 1)[1]

    # ---- property walking; offsets reported are offsets in the .uexp file ----
    def props(self, e):
        start = e['off'] - self.header_size
        r = R(self.ue, start)
        return self._props(r), r.p

    def _props(self, r):
        out = []
        while True:
            tag_pos = r.p
            name = self.fname(r)
            if name == 'None':
                break
            typ = self.fname(r)
            size_pos = r.p
            size, aidx = r.u('ii')
            extra = {}
            if typ == 'StructProperty':
                extra['struct'] = self.fname(r); r.p += 16
            elif typ == 'BoolProperty':
                extra['bool'] = r.u('B')
            elif typ in ('ByteProperty', 'EnumProperty'):
                extra['enum'] = self.fname(r)
            elif typ in ('ArrayProperty', 'SetProperty'):
                extra['inner'] = self.fname(r)
            elif typ == 'MapProperty':
                extra['inner'] = self.fname(r); extra['value'] = self.fname(r)
            if r.u('B'):
                r.p += 16
            vpos = r.p
            val = self._value(r, typ, size, extra)
            r.p = vpos + size
            out.append(dict(name=name, type=typ, size=size, size_pos=size_pos, aidx=aidx, pos=vpos, val=val,
                            tag_pos=tag_pos, **extra))
        return out

    def _value(self, r, typ, size, extra):
        try:
            if typ == 'FloatProperty':
                return r.u('f')
            if typ == 'IntProperty':
                return r.u('i')
            if typ == 'BoolProperty':
                return bool(extra['bool'])
            if typ == 'ObjectProperty':
                return self.ref(r.u('i'))
            if typ == 'NameProperty':
                return self.fname(r)
            if typ == 'StrProperty':
                return r.fstr()
            if typ == 'EnumProperty':
                return self.fname(r)
            if typ == 'ByteProperty':
                return self.fname(r) if size == 8 else r.u('B')
            if typ == 'StructProperty':
                return self._struct(r, extra['struct'], size)
            if typ == 'ArrayProperty':
                n = r.u('i')
                inner = extra['inner']
                if inner == 'StructProperty':
                    self.fname(r); self.fname(r); r.u('ii')
                    sn = self.fname(r); r.p += 16; r.u('B')
                    return [self._struct(r, sn, None) for _ in range(n)]
                if inner == 'ObjectProperty':
                    return [self.ref(r.u('i')) for _ in range(n)]
                if inner == 'FloatProperty':
                    return [r.u('f') for _ in range(n)]
                return f'<array {inner} x{n}>'
        except Exception as ex:  # keep walking; size tells us where the next tag is
            return f'<err {ex}>'
        return f'<{typ} {size}b>'

    def _struct(self, r, sn, size):
        if sn in NATIVE_STRUCTS:
            pos = r.p
            v = r.u(NATIVE_STRUCTS[sn])
            return dict(_native=sn, _pos=pos, v=v)
        if sn in ('SoftObjectPath', 'SoftClassPath'):
            return dict(_native=sn, v=(self.fname(r), r.fstr()))
        return self._props(r)


def fmt(v, ind=0):
    pad = '  ' * ind
    lines = []
    if isinstance(v, list) and v and isinstance(v[0], dict) and 'name' in v[0]:
        for p in v:
            val = p['val']
            head = f"{pad}{p['name']}" + (f"[{p['aidx']}]" if p['aidx'] else '') + f" ({p.get('struct') or p['type'].replace('Property', '')})"
            if isinstance(val, list) and val and isinstance(val[0], (dict, list)):
                lines.append(head + ':')
                if isinstance(val[0], list):
                    for k, el in enumerate(val):
                        lines.append(f'{pad}  [{k}]'); lines += fmt(el, ind + 2)
                else:
                    lines += fmt(val, ind + 1)
            elif isinstance(val, dict):
                lines.append(f"{head} = {val['v']}" + (f"  @0x{val['_pos']:x}" if '_pos' in val else ''))
            else:
                at = f"  @0x{p['pos']:x}" if p['type'] in ('FloatProperty', 'IntProperty') else ''
                lines.append(f'{head} = {val}{at}')
    elif isinstance(v, list):
        for k, el in enumerate(v):
            lines.append(f'{pad}[{k}] {el}')
    return lines


if __name__ == '__main__':
    pk = Package(sys.argv[1])
    want = sys.argv[2:]
    print(f'{os.path.basename(pk.base)}: names={pk.name_count} imports={pk.import_count} exports={pk.export_count} header={pk.header_size}')
    for e in pk.exports:
        c = pk.cls(e)
        if want and not any(w.lower() in (c + ' ' + e['name']).lower() for w in want):
            continue
        print(f"\n#{e['i']} {c} '{e['name']}' outer={pk.ref(e['outer'])} size={e['size']} uexp@0x{e['off'] - pk.header_size:x}")
        try:
            pr, _ = pk.props(e)
            print('\n'.join(fmt(pr, 1)))
        except Exception as ex:
            print('   <parse error>', ex)
