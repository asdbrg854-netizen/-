#!/usr/bin/env python3
"""Compile Warzone 2100 .po files into .mo catalogs for the Android built-in reader.

Usage (from the repository root):
    python3 platforms/android/tools/po2mo.py

Reads  po/<lang>.po         -> platforms/android/locale/<dir>/LC_MESSAGES/warzone2100.mo
       po/guide/<lang>.po   -> platforms/android/locale/<dir>/LC_MESSAGES/warzone2100_guide.mo
The <dir> names match the "localeFilename" column of the language table in lib/framework/i18n.cpp.
Fuzzy and untranslated entries are skipped (same as msgfmt's default behaviour).
"""
import ast
import json
import mobile_strings
import os
import re
import struct
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
PO_DIR = os.path.join(ROOT, 'po')
OUT_DIR = os.path.join(ROOT, 'platforms', 'android', 'locale')

# Languages that can be chosen in the game (localeFilename -> candidate .po basenames).
# Keep in sync with the Unix language table in lib/framework/i18n.cpp.
LANGUAGES = {
    'ar_SA': ['ar_SA'], 'bg_BG': ['bg_BG'], 'ca_ES': ['ca_ES'], 'cs': ['cs'], 'da': ['da'],
    'de': ['de'], 'el': ['el'], 'en_GB': ['en_GB'], 'es': ['es'], 'et_EE': ['et_EE'],
    'fi': ['fi'], 'fr': ['fr'], 'fy': ['fy'], 'ga': ['ga'], 'hr': ['hr'], 'hu': ['hu'],
    'id': ['id'], 'it': ['it'], 'ja_JP': ['ja_JP'], 'ko': ['ko'], 'la': ['la'], 'lt': ['lt'], 'nb': ['nb'],
    'nl': ['nl'], 'pl': ['pl'], 'pt_BR': ['pt_BR'], 'pt': ['pt'], 'ro': ['ro'], 'ru': ['ru'],
    'sk': ['sk'], 'sl': ['sl'], 'sr_RS': ['sr_RS', 'sr_CS'], 'tr': ['tr'], 'uk_UA': ['uk_UA'],
    'zh_CN': ['zh_CN'], 'zh_TW': ['zh_TW'],
}


def decode_po_escapes(s):
    """Decode a string written with .po escapes (\\n, \\", \\\\ ...); a bare quote is accepted too."""
    s = re.sub(r'(?<!\\)"', r'\\"', s)
    return ast.literal_eval('"' + s + '"')


def unquote(s):
    s = s.strip()
    if not (s.startswith('"') and s.endswith('"')):
        raise ValueError('bad string: ' + s)
    # po escapes are a subset of Python's; ast handles \n \t \" \\ and octal.
    return ast.literal_eval(s)


def parse_po(path):
    """Return dict: key(bytes) -> value(bytes) for translated, non-fuzzy entries (+ header)."""
    messages = {}
    ctxt = msgid = msgid_plural = None
    strs = {}
    section = None
    fuzzy = False
    in_entry = False

    def flush():
        nonlocal ctxt, msgid, msgid_plural, strs, section, fuzzy, in_entry
        if in_entry and msgid is not None:
            is_header = (msgid == '' and ctxt is None)
            if msgid_plural is not None:
                forms = [strs[k] for k in sorted(strs)]
                translated = any(f != '' for f in forms)
                value = '\0'.join(forms)
                key = msgid + '\0' + msgid_plural
            else:
                value = strs.get(0, '')
                translated = value != ''
                key = msgid
            if ctxt is not None:
                key = ctxt + '\x04' + key
            if translated and (is_header or not fuzzy):
                messages[key.encode('utf-8')] = value.encode('utf-8')
        ctxt = msgid = msgid_plural = None
        strs = {}
        section = None
        fuzzy = False
        in_entry = False

    with open(path, encoding='utf-8') as f:
        for raw in f:
            line = raw.strip()
            if line.startswith('#~'):
                continue  # obsolete
            if line == '':
                flush()
                continue
            if line.startswith('#'):
                if line.startswith('#,') and 'fuzzy' in line:
                    if in_entry and msgid is not None and section is not None:
                        flush()
                    fuzzy = True
                continue
            m = re.match(r'^(msgctxt|msgid_plural|msgid|msgstr(?:\[(\d+)\])?)\s+(".*")$', line)
            if m:
                kw = m.group(1)
                val = unquote(m.group(3))
                if kw == 'msgctxt':
                    if in_entry and (msgid is not None):
                        flush()
                    ctxt, section, in_entry = val, 'ctxt', True
                elif kw == 'msgid':
                    if in_entry and msgid is not None:
                        flush()
                    msgid, section, in_entry = val, 'id', True
                elif kw == 'msgid_plural':
                    msgid_plural, section = val, 'idp'
                else:
                    idx = int(m.group(2)) if m.group(2) is not None else 0
                    strs[idx] = val
                    section = ('str', idx)
                continue
            if line.startswith('"'):
                val = unquote(line)
                if section == 'ctxt':
                    ctxt += val
                elif section == 'id':
                    msgid += val
                elif section == 'idp':
                    msgid_plural += val
                elif isinstance(section, tuple):
                    strs[section[1]] += val
                continue
        flush()
    return messages


def write_mo(messages, path):
    # Sort by the part of the key before the first NUL (strcmp order used by the reader's binary search)
    items = sorted(messages.items(), key=lambda kv: kv[0].split(b'\0', 1)[0])
    n = len(items)
    keys_blob = b''
    vals_blob = b''
    key_offsets = []
    val_offsets = []
    header_size = 28
    table_size = n * 8
    keys_start = header_size + 2 * table_size
    for k, _ in items:
        key_offsets.append((len(k), keys_start + len(keys_blob)))
        keys_blob += k + b'\0'
    vals_start = keys_start + len(keys_blob)
    for _, v in items:
        val_offsets.append((len(v), vals_start + len(vals_blob)))
        vals_blob += v + b'\0'
    out = struct.pack('<IIIIIII', 0x950412de, 0, n, header_size, header_size + table_size, 0, 0)
    for l, o in key_offsets:
        out += struct.pack('<II', l, o)
    for l, o in val_offsets:
        out += struct.pack('<II', l, o)
    out += keys_blob + vals_blob
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as f:
        f.write(out)
    return n


def find_po(directory, candidates):
    for c in candidates:
        p = os.path.join(directory, c + '.po')
        if os.path.isfile(p):
            return p
    return None


def main():
    total = 0
    for locale_dir, candidates in sorted(LANGUAGES.items()):
        for domain, subdir in (('warzone2100', PO_DIR), ('warzone2100_guide', os.path.join(PO_DIR, 'guide'))):
            po = find_po(subdir, candidates)
            if not po:
                continue
            msgs = parse_po(po)
            if domain == 'warzone2100':
                # Strings added by Warzone2100 Mobile (see mobile_strings.py); translations from po/ win
                for msgid, text in mobile_strings.for_locale(locale_dir).items():
                    msgs.setdefault(msgid.encode('utf-8'), text.encode('utf-8'))
                # Extra translations kept outside po/ (platforms/android/tools/translations/<dir>.json);
                # keys and values use the same escaping as in a .po file. po/ translations win.
                extra_path = os.path.join(os.path.dirname(__file__), 'translations', locale_dir + '.json')
                if os.path.exists(extra_path):
                    for msgid, text in json.load(open(extra_path, encoding='utf-8')).items():
                        key = decode_po_escapes(msgid).encode('utf-8')
                        if not msgs.get(key):
                            msgs[key] = decode_po_escapes(text).encode('utf-8')
            out = os.path.join(OUT_DIR, locale_dir, 'LC_MESSAGES', domain + '.mo')
            count = write_mo(msgs, out)
            total += os.path.getsize(out)
            print('%-6s %-18s %5d messages  %8d bytes' % (locale_dir, domain, count, os.path.getsize(out)))
    print('total %.1f MiB' % (total / 1048576.0))


if __name__ == '__main__':
    sys.exit(main())
