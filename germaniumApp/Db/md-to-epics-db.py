#!/usr/bin/env python3
# md_to_epics_db.py — Convert PV Markdown tables to EPICS .db
# - Supports both 5-col tables:    PV | TYPE | ADDR | VALUE | NOTE
# - And 6-col tables with SIZE:    PV | TYPE | ADDR | SIZE | VALUE | NOTE
# - Input types:  ai, bi, longin, waveformin, stringin
# - Output types: ao, bo, longout, waveformout, waveform, stringout, mbbo
# - NDArray rows are skipped (comment emitted if wanted; currently ignored)
# - Waveforms: record type "waveform", FTVL="LONG", SCAN="I/O Intr"
#     DTYP: waveformin → asynInt32ArrayIn; waveformout → asynInt32ArrayOut; waveform → none
#     NELM: from SIZE column if present; else default 16384
#
# Links:
#   Inputs  → field(INP,  "@asyn($(PORT),{ADDR}){PV}")
#   Outputs → field(OUT,  "@asyn($(PORT),{ADDR}){PV}")
#
# bo:   add ZNAM/ONAM if VALUE has 0/1 labels (default Off/On)
# bi:   convenience ZNAM/ONAM if labels provided
# mbbo: populate any provided indices 0..15 into ZRST/ZRVL .. FFST/FFVL
#
# Usage:
#   python md_to_epics_db.py INPUT.md > out.db
#   cat INPUT.md | python md_to_epics_db.py > out.db

import sys, re
from typing import List, Tuple, Dict

def split_md_row(line: str) -> List[str]:
    s = line.strip()
    if s.startswith("|"): s = s[1:]
    if s.endswith("|"): s = s[:-1]
    return [c.strip() for c in s.split("|")]

def normalize_breaks(s: str) -> str:
    return s.replace("<br/>","\n").replace("<br>","\n").replace("<BR>","\n")

def parse_enum_values(cell: str) -> Dict[int, str]:
    text = normalize_breaks(cell)
    out: Dict[int,str] = {}
    for raw in text.splitlines():
        t = raw.strip()
        if not t:
            continue
        m = re.match(r"^\s*(\d+)\s*[:\-]\s*(.+?)\s*$", t)
        if m:
            out[int(m.group(1))] = m.group(2).strip()
    return dict(sorted(out.items(), key=lambda kv: kv[0]))

def emit_common_header() -> str:
    return "# Auto-generated from Markdown PV table\n# Macros expected: P, PORT\n\n"

def link_field_name(rec_type: str) -> str:
    t = rec_type.lower()
    if t in ("ai","bi","longin","waveformin","stringin"):
        return "INP"
    return "OUT"

def map_record_type(typ: str) -> Tuple[str, str]:
    t = (typ or "").lower()
    if t == "waveformin":
        return ("waveform", '    field(DTYP, "asynInt32ArrayIn")')
    if t == "waveformout":
        return ("waveform", '    field(DTYP, "asynInt32ArrayOut")')
    if t == "waveform":
        return ("waveform", "")
    if t == "stringin":
        return ("stringin", '    field(DTYP, "asynOctetRead")')
    if t == "stringout":
        return ("stringout", '    field(DTYP, "asynOctetWrite")')
    return (t, "")

def emit_waveform_defaults(nelm: str|None) -> List[str]:
    parts = ['    field(FTVL, "LONG")']
    parts.append(f'    field(NELM, "{nelm if nelm else "16384"}")')
    parts.append('    field(SCAN, "I/O Intr")')
    return parts

def gen_bo(pv: str, addr: str, value: str, note: str) -> str:
    enums = parse_enum_values(value)
    znam = enums.get(0, "Off"); onam = enums.get(1, "On")
    body = [
        f'record(bo, "$(P)$(R){pv}") {{',
        f'    field(DTYP, "asynInt32")',
        f'    field(OUT,  "@asyn($(PORT),{addr}){pv}")',
    ]
    if note: body.append(f'    field(DESC, "{note}")')
    body.append(f'    field(ZNAM, "{znam}")')
    body.append(f'    field(ONAM, "{onam}")')
    body.append("}")
    return "\n".join(body)

def gen_mbbo(pv: str, addr: str, value: str, note: str) -> str:
    enums = parse_enum_values(value)
    order = [
        ("ZRST","ZRVL",0), ("ONST","ONVL",1), ("TWST","TWVL",2), ("THST","THVL",3),
        ("FRST","FRVL",4), ("FVST","FVVL",5), ("SXST","SXVL",6), ("SVST","SVVL",7),
        ("EIST","EIVL",8), ("NIST","NIVL",9), ("TENST","TEVL",10), ("ELST","ELVL",11),
        ("TVST","TVVL",12), ("TTST","TTVL",13), ("FTST","FTVL",14), ("FFST","FFVL",15),
    ]
    body = [
        f'record(mbbo, "$(P)$(R){pv}") {{',
        f'    field(DTYP, "asynInt32")',
        f'    field(OUT,  "@asyn($(PORT),{addr}){pv}")',
    ]
    if note: body.append(f'    field(DESC, "{note}")')
    for st, vl, idx in order:
        if idx in enums:
            lab = enums[idx][:26]
            body.append(f'    field({st}, "{lab}")')
            body.append(f'    field({vl}, "{idx}")')
    body.append("}")
    return "\n".join(body)

def gen_numeric(rectype: str, pv: str, addr: str, note: str, link: str, dtyp: str|None="") -> str:
    body = [f'record({rectype}, "$(P)$(R){pv}") {{']
    if dtyp: body.append(f'    field(DTYP, "{dtyp}")')
    body.append(f'    field({link},  "@asyn($(PORT),{addr}){pv}")')
    if note: body.append(f'    field(DESC, "{note}")')
    body.append("}")
    return "\n".join(body)

def gen_waveform(typ: str, pv: str, addr: str, note: str, nelm: str|None) -> str:
    print(f'waveform: {typ}');
    rectype, dtyp = map_record_type(typ)
    link = "INP" if typ.lower() == "waveformin" else "OUT"
    #body = [f'record(waveform, "$(P)$(R){pv}") {{']
    body = [
        f'record(waveform, "$(P)$(R){pv}") {{',
        f'    field(DTYP, "asynInt32Array")',
        f'    field(OUT,  "@asyn($(PORT),{addr}){pv}")',
    ]
    body += emit_waveform_defaults(nelm)
    if dtyp: body.append(dtyp)
    body.append(f'    field({link},  "@asyn($(PORT),{addr}){pv}")')
    if note: body.append(f'    field(DESC, "{note}")')
    body.append("}")
    return "\n".join(body)

def parse_tables(md_text: str):
    lines = md_text.splitlines()
    i = 0
    # Yield dicts with keys: pv, type, addr, size, value, note
    while i < len(lines):
        line = lines[i]
        if "|" in line and re.search(r"\bPV\b", line, re.I) and re.search(r"\bType\b|\bTYPE\b", line, re.I):
            six = "SIZE" in line.upper()
            i += 2  # skip header + separator
            while i < len(lines) and "|" in lines[i]:
                row = split_md_row(lines[i]); i += 1
                if six:
                    if len(row) < 6: continue
                    pv, typ, addr, size, val, note = (row + ["","",""])[:6]
                else:
                    if len(row) < 5: continue
                    pv, typ, addr, val, note = (row + ["","",""])[:5]
                    size = ""
                yield {"pv": pv.strip(), "type": typ.strip(), "addr": addr.strip(),
                       "size": size.strip(), "value": val.strip(), "note": note.strip()}
            continue
        i += 1

def generate_db(md_text: str) -> str:
    parts = [emit_common_header()]
    for ent in parse_tables(md_text):
        typ = ent["type"].lower()
        if typ in ("", "ndarray"):
            continue  # skip NDArray / empty
        if typ == "bo":
            parts.append(gen_bo(ent["pv"], ent["addr"], ent["value"], ent["note"]))
        elif typ == "mbbo":
            parts.append(gen_mbbo(ent["pv"], ent["addr"], ent["value"], ent["note"]))
        elif typ == "ai":
            parts.append(gen_numeric(typ, ent["pv"], ent["addr"], ent["note"], "INP", "asynFloat64"))
        elif typ in ("bi","longin"):
            parts.append(gen_numeric(typ, ent["pv"], ent["addr"], ent["note"], "INP", "asynInt32"))
        elif typ == "ao":
            parts.append(gen_numeric(typ, ent["pv"], ent["addr"], ent["note"], "OUT", "asynFloat64"))
        elif typ == "longout":
            parts.append(gen_numeric(typ, ent["pv"], ent["addr"], ent["note"], "OUT", "asynInt32"))
        elif typ in ("stringin","stringout"):
            rectype, dtyp = map_record_type(typ)
            link = link_field_name(typ)
            parts.append(gen_numeric(rectype, ent["pv"], ent["addr"], ent["note"], link, dtyp))
        elif typ in ("waveform","waveformout","waveformin"):
            parts.append(gen_waveform(typ, ent["pv"], ent["addr"], ent["note"], ent["size"] or None))
        else:
            parts.append(f"/* Unsupported type '{ent['type']}' for PV {ent['pv']} */")
        parts.append("")
    return "\n".join(parts).rstrip() + "\n"

def main():
    data = sys.stdin.read() if not sys.stdin.isatty() else (open(sys.argv[1], "r", encoding="utf-8").read() if len(sys.argv) > 1 else "")
    #sys.stdout.write(generate_db(data))
    with open(sys.argv[2], "w") as f:
        f.write(generate_db(data))

if __name__ == "__main__":
    main()
