#!/usr/bin/env python3
"""Translation coverage of the catalogs that ship in the app (the compiled .mo files).

    python3 platforms/android/tools/po2mo.py            # build the catalogs first
    python3 platforms/android/tools/po_coverage.py      # coverage of every language
    python3 platforms/android/tools/po_coverage.py ar_SA --missing   # list untranslated texts of a language

Counts how many messages of po/warzone2100.pot have a non-empty translation in
platforms/android/locale/<lang>/LC_MESSAGES/warzone2100.mo.
"""
import gettext, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import po2mo


def pot_msgids():
    txt = open(os.path.join(po2mo.PO_DIR, 'warzone2100.pot'), encoding='utf-8').read()
    ids = []
    for entry in re.split(r'\n\n+', txt):
        m = re.search(r'^msgid\s+((?:"(?:[^"\\]|\\.)*"\s*\n?)+)', entry, re.M)
        if not m:
            continue
        raw = ''.join(re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(1)))
        if raw:
            msgid = po2mo.decode_po_escapes(raw)
            cm = re.search(r'^msgctxt\s+((?:"(?:[^"\\]|\\.)*"\s*\n?)+)', entry, re.M)
            if cm:
                ctxt = po2mo.decode_po_escapes(''.join(re.findall(r'"((?:[^"\\]|\\.)*)"', cm.group(1))))
                msgid = ctxt + '\x04' + msgid
            ids.append(msgid)
    return ids


def mobile_ids():
    import mobile_strings
    return list(mobile_strings.MSGIDS) + list(mobile_strings.EXTRA_IDS)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    show_missing = '--missing' in sys.argv
    ids = pot_msgids()
    rows = []
    for lang in sorted(po2mo.LANGUAGES):
        if args and lang not in args:
            continue
        path = os.path.join(po2mo.OUT_DIR, lang, 'LC_MESSAGES', 'warzone2100.mo')
        if not os.path.exists(path):
            continue
        catalog = gettext.GNUTranslations(open(path, 'rb'))._catalog
        everything = ids + mobile_ids()
        missing = [m for m in everything if not catalog.get(m)]
        rows.append((lang, len(everything) - len(missing), len(everything)))
        if show_missing:
            for m in missing:
                print(repr(m))
    if not show_missing:
        for lang, done, total in rows:
            print('%-8s %5d / %5d  %5.1f%%' % (lang, done, total, 100.0 * done / total))


main()
