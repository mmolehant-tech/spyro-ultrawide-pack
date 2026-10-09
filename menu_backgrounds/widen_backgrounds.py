#!/usr/bin/env python3
"""Let the shared menu backgrounds and the full-screen overlays (vignettes, fades
to black) of Spyro Reignited Trilogy overflow the fixed 1920x1080 menu canvas on
the left and right, so they cover an ultrawide screen.

Usage: widen_backgrounds.py <orig root> <out root> <overflow in canvas units>
"""
import glob, os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from uasset import Package

BG = {'UI_Background_000_C', 'UI_Background_001_C', 'UI_Title_Background_C'}
BULK_POS = 169  # int64 BulkDataStartOffset in the package summary (verified for these files)


def find(props, name):
    for p in props:
        if p['name'] == name:
            return p
    return None


def fname(pk, name):
    return struct.pack('<ii', pk.names.index(name), 0)


def insert(pk, export, pos, data, size_fields):
    """Insert bytes into the .uexp and fix every size/offset that depends on it."""
    assert struct.unpack_from('<q', pk.ua, BULK_POS)[0] == len(pk.ua) + len(pk.ue) - 4
    pk.ue[pos:pos] = data
    n = len(data)
    for sp in size_fields:  # enclosing property tags
        struct.pack_into('<i', pk.ue, sp, struct.unpack_from('<i', pk.ue, sp)[0] + n)
    struct.pack_into('<q', pk.ua, export['entry_pos'] + 28, export['size'] + n)
    for e in pk.exports:
        if e['off'] > export['off']:
            struct.pack_into('<q', pk.ua, e['entry_pos'] + 36, e['off'] + n)
    struct.pack_into('<q', pk.ua, BULK_POS, struct.unpack_from('<q', pk.ua, BULK_POS)[0] + n)
    pk.parse()


def add_to_margin(pk, idx, side, delta):
    """Offsets.<side> += delta on a CanvasPanelSlot, inserting the property when it has its default (0)."""
    e = pk.exports[idx]
    props, _ = pk.props(e)
    layout = find(props, 'LayoutData')
    offsets = find(layout['val'], 'Offsets')
    cur = find(offsets['val'], side)
    if cur:
        struct.pack_into('<f', pk.ue, cur['pos'], cur['val'] + delta)
        return 'patched'
    tag = fname(pk, side) + fname(pk, 'FloatProperty') + struct.pack('<iiBf', 4, 0, 0, delta)
    insert(pk, e, offsets['pos'], tag, [offsets['size_pos'], layout['size_pos']])
    return 'inserted'


def background_slots(pk):
    """Indices of horizontally stretched CanvasPanelSlots that belong to a background widget tree."""
    out = []
    for e in pk.exports:
        if pk.cls(e) != 'CanvasPanelSlot' or e['outer'] <= 0:
            continue
        panel = pk.exports[e['outer'] - 1]
        tree = pk.exports[panel['outer'] - 1] if panel['outer'] > 0 else None
        if not tree or pk.cls(tree) != 'WidgetTree' or tree['outer'] <= 0:
            continue
        owner = pk.exports[tree['outer'] - 1]
        if pk.cls(owner) not in BG and owner['name'] not in BG:
            continue
        props, _ = pk.props(e)
        layout = find(props, 'LayoutData')
        if not layout:
            continue
        anchors = find(layout['val'], 'Anchors')
        amin = find(anchors['val'], 'Minimum') if anchors else None
        amax = find(anchors['val'], 'Maximum') if anchors else None
        min_x = amin['val']['v'][0] if amin else 0.0
        max_x = amax['val']['v'][0] if amax else 0.0
        if not (min_x == 0.0 and max_x == 1.0):
            continue  # not stretched across the canvas (corner decorations)
        offsets = find(layout['val'], 'Offsets')
        right = find(offsets['val'], 'Right')
        if right and abs(right['val']) > 1000:
            continue  # glow that is parked off-screen on purpose
        out.append(e['i'])
    return out


VIGNETTES = {'UI_CornerVignettes_001', 'UI_CornerVignettes_002'}
BLACK_LAYERS = {'Black', 'BlackBackground', 'FadeToBlack'}


def is_black(color):
    """True for a LinearColor property whose red, green and blue are all zero."""
    return bool(color) and tuple(color['val']['v'][:3]) == (0.0, 0.0, 0.0)


def overlay_slots(pk):
    """Indices of CanvasPanelSlots holding a full-screen vignette or fade layer.

    Only slots directly on the root canvas of a widget that fill it exactly are taken,
    so small local images are never touched."""
    out = []
    for e in pk.exports:
        if pk.cls(e) != 'CanvasPanelSlot' or e['outer'] <= 0:
            continue
        panel = pk.exports[e['outer'] - 1]
        tree = pk.exports[panel['outer'] - 1] if panel['outer'] > 0 else None
        if not tree or pk.cls(tree) != 'WidgetTree' or tree['outer'] <= 0:
            continue
        owner = pk.exports[tree['outer'] - 1]
        if pk.cls(owner) in BG or owner['name'] in BG:
            continue
        root = find(pk.props(tree)[0], 'RootWidget')
        if not root or root['val'] != pk.ref(panel['i'] + 1):
            continue
        props, _ = pk.props(e)
        layout = find(props, 'LayoutData')
        content = find(props, 'Content')
        if not layout or not content or not content['val'].startswith('exp'):
            continue
        anchors = find(layout['val'], 'Anchors')
        amin = find(anchors['val'], 'Minimum') if anchors else None
        amax = find(anchors['val'], 'Maximum') if anchors else None
        if (amin['val']['v'][0] if amin else 0.0) != 0.0 or (amax['val']['v'][0] if amax else 0.0) != 1.0:
            continue
        offsets = find(layout['val'], 'Offsets')
        left, right = find(offsets['val'], 'Left'), find(offsets['val'], 'Right')
        if (left and left['val'] != 0.0) or not right or right['val'] != 0.0:
            continue
        layer = pk.exports[int(content['val'].split(':')[0][3:])]
        lp = pk.props(layer)[0]
        kind = pk.cls(layer)
        if kind == 'Image':
            brush = find(lp, 'Brush')
            res = find(brush['val'], 'ResourceObject') if brush else None
            res_name = res['val'].split(':')[-1] if res else None
            tint = find(brush['val'], 'TintColor') if brush else None
            tint = find(tint['val'], 'SpecifiedColor') if tint else None
            if res_name in VIGNETTES:
                out.append(e['i'])
            elif res_name is None and (layer['name'] in BLACK_LAYERS or is_black(find(lp, 'ColorAndOpacity'))
                                       or is_black(tint)):
                out.append(e['i'])
        elif kind == 'Border':
            # an empty border used as a fade-to-black layer
            background = find(lp, 'Background')
            has_res = background and find(background['val'], 'ResourceObject')
            if not find(lp, 'Slots') and not has_res and is_black(find(lp, 'BrushColor')):
                out.append(e['i'])
    return out


def contiguous(pk):
    ex = sorted(pk.exports, key=lambda e: e['off'])
    return (ex[0]['off'] == pk.header_size
            and all(a['off'] + a['size'] == b['off'] for a, b in zip(ex, ex[1:]))
            and ex[-1]['off'] + ex[-1]['size'] == len(pk.ua) + len(pk.ue) - 4)


def main(orig, out, overflow):
    done = 0
    for f in sorted(glob.glob(f'{orig}/**/*.uasset', recursive=True)):
        try:
            pk = Package(f)
        except Exception:
            continue
        before = {e['i']: (pk.cls(e), e['name'], e['size']) for e in pk.exports}
        try:
            bg, ov = background_slots(pk), overlay_slots(pk)
        except Exception:
            continue  # not a widget package this reader understands
        slots = bg + [i for i in ov if i not in bg]
        if not slots:
            continue
        if 'Left' not in pk.names or 'FloatProperty' not in pk.names:
            # a missing margin can only be added when the package already knows these names
            print(f"SKIPPED (no 'Left' name in package): {os.path.relpath(f, orig)[:-7].split('Content/')[-1]}")
            continue
        stats = {'patched': 0, 'inserted': 0}
        for idx in slots:
            for side in ('Left', 'Right'):
                stats[add_to_margin(pk, idx, side, -overflow)] += 1
        # sanity: layout still consistent and every export still parses
        assert contiguous(pk), f
        for e in pk.exports:
            pk.props(e)
            assert (pk.cls(e), e['name']) == before[e['i']][:2]
        rel = os.path.relpath(f, orig)[:-7]
        os.makedirs(os.path.dirname(f'{out}/{rel}'), exist_ok=True)
        open(f'{out}/{rel}.uasset', 'wb').write(pk.ua)
        open(f'{out}/{rel}.uexp', 'wb').write(pk.ue)
        done += 1
        print(f"{rel.split('Content/')[-1]:44s} background={len(bg):3d} overlay={len(ov):2d} in-place={stats['patched']:3d} inserted={stats['inserted']:3d}")
    print(done, 'files written')


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2], float(sys.argv[3]))
