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

mbbx_order = [
    ("ZRST","ZRVL",0), ("ONST","ONVL",1), ("TWST","TWVL",2), ("THST","THVL",3),
    ("FRST","FRVL",4), ("FVST","FVVL",5), ("SXST","SXVL",6), ("SVST","SVVL",7),
    ("EIST","EIVL",8), ("NIST","NIVL",9), ("TENST","TEVL",10), ("ELST","ELVL",11),
    ("TVST","TVVL",12), ("TTST","TTVL",13), ("FTST","FTVL",14), ("FFST","FFVL",15),
]

#---------------------------------------------------------------------------#

def split_md_row(line: str) -> List[str]:
    s = line.strip()
    if s.startswith("|"): s = s[1:]
    if s.endswith("|"): s = s[:-1]
    return [c.strip() for c in s.split("|")]

#---------------------------------------------------------------------------#

def normalize_breaks(s: str) -> str:
    return s.replace("<br/>","\n").replace("<br>","\n").replace("<BR>","\n")

#---------------------------------------------------------------------------#

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

#---------------------------------------------------------------------------#

def emit_common_header() -> str:
    return "# Auto-generated from Markdown PV table\n# Macros expected: P, R, PORT\n\n"

#---------------------------------------------------------------------------#

def link_field_name(rec_type: str) -> str:
    t = rec_type.lower()
    if t in ("ai","bi","longin","waveformin","stringin"):
        return "INP"
    return "OUT"

#---------------------------------------------------------------------------#

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

#---------------------------------------------------------------------------#

def emit_waveform_defaults(nelm: str|None) -> List[str]:
    parts = ['    field(FTVL, "LONG")']
    parts.append(f'    field(NELM, "{nelm if nelm else "16384"}")')
    parts.append('    field(SCAN, "I/O Intr")')
    return parts

#---------------------------------------------------------------------------#

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

#---------------------------------------------------------------------------#

def gen_bi(pv: str, addr: str, value: str, note: str) -> str:
    enums = parse_enum_values(value)
    znam = enums.get(0, "Off"); onam = enums.get(1, "On")
    body = [
        f'record(bi, "$(P)$(R){pv}") {{',
        f'    field(DTYP, "asynInt32")',
        f'    field(INP,  "@asyn($(PORT),{addr}){pv}")',
    ]
    if note: body.append(f'    field(DESC, "{note}")')
    body.append(f'    field(ZNAM, "{znam}")')
    body.append(f'    field(ONAM, "{onam}")')
    body.append(f'    field(SCAN, "I/O Intr")')
    body.append("}")
    return "\n".join(body)

#---------------------------------------------------------------------------#

def gen_mbbo(pv: str, addr: str, value: str, note: str) -> str:
    enums = parse_enum_values(value)
    body = [
        f'record(mbbo, "$(P)$(R){pv}") {{',
        f'    field(DTYP, "asynInt32")',
        f'    field(OUT,  "@asyn($(PORT),{addr}){pv}")',
    ]
    if note: body.append(f'    field(DESC, "{note}")')
    for st, vl, idx in mbbx_order:
        if idx in enums:
            lab = enums[idx][:26]
            body.append(f'    field({st}, "{lab}")')
            body.append(f'    field({vl}, "{idx}")')
    body.append("}")
    return "\n".join(body)

#---------------------------------------------------------------------------#

def gen_mbbi(pv: str, addr: str, value: str, note: str) -> str:
    enums = parse_enum_values(value)
    body = [
        f'record(mbbi, "$(P)$(R){pv}") {{',
        f'    field(DTYP, "asynInt32")',
        f'    field(INP,  "@asyn($(PORT),{addr}){pv}")',
    ]
    if note: body.append(f'    field(DESC, "{note}")')
    for st, vl, idx in mbbx_order:
        if idx in enums:
            lab = enums[idx][:26]
            body.append(f'    field({st}, "{lab}")')
            body.append(f'    field({vl}, "{idx}")')
    body.append(f'    field(SCAN, "I/O Intr")')
    body.append("}")
    return "\n".join(body)

#---------------------------------------------------------------------------#

def gen_numeric(rectype: str, pv: str, addr: str, note: str, link: str, dtyp: str|None="") -> str:
    body = [f'record({rectype}, "$(P)$(R){pv}") {{']
    if dtyp: body.append(f'    field(DTYP, "{dtyp}")')
    body.append(f'    field({link},  "@asyn($(PORT),{addr}){pv}")')
    if note: body.append(f'    field(DESC, "{note}")')
    # Add SCAN for input records
    if link == "INP":
        body.append('    field(SCAN, "I/O Intr")')
    body.append("}")
    return "\n".join(body)

#---------------------------------------------------------------------------#

def gen_waveform(typ: str, pv: str, addr: str, note: str, nelm: str|None) -> str:
    rectype, dtyp_extra = map_record_type(typ)
    link_name = "INP" if typ.lower() == "waveformin" else "OUT"
    
    body = [f'record(waveform, "$(P)$(R){pv}") {{']
    
    # Set DTYP based on type
    if typ.lower() == "waveformin":
        body.append('    field(DTYP, "asynInt32ArrayIn")')
    elif typ.lower() == "waveformout":
        body.append('    field(DTYP, "asynInt32ArrayOut")')
    else:
        body.append('    field(DTYP, "asynInt32Array")')
    
    # Add link field
    body.append(f'    field({link_name},  "@asyn($(PORT),{addr}){pv}")')
    
    # Add waveform defaults
    body += emit_waveform_defaults(nelm)
    
    # Add description if provided
    if note: 
        body.append(f'    field(DESC, "{note}")')
    
    body.append("}")
    return "\n".join(body)

#---------------------------------------------------------------------------#

def parse_tables(md_text: str):
    lines = md_text.splitlines()
    i = 0
    # Yield dicts with keys: pv, type, addr, size, value, note
    while i < len(lines):
        line = lines[i].strip()
        # Look for table headers containing PV and TYPE columns
        if "|" in line and re.search(r"\bPV\b", line, re.I) and re.search(r"\bType\b|\bTYPE\b", line, re.I):
            header = split_md_row(line)
            header_lower = [col.lower().strip() for col in header]
            
            # Skip the separator line (typically :-:|:-:|:-: etc.)
            i += 1
            if i < len(lines) and "|" in lines[i] and ":" in lines[i]:
                i += 1
            
            # Process data rows
            while i < len(lines):
                line = lines[i].strip()
                # Stop if we hit an empty line, heading, or non-table content
                if not line or not "|" in line or line.startswith("#"):
                    break
                    
                row = split_md_row(line)
                if len(row) < 4:  # Need at least PV, TYPE, something, something
                    i += 1
                    continue
                
                # Initialize default values
                pv = typ = addr = size = val = note = ""
                
                # Map columns based on header
                for idx, cell in enumerate(row):
                    if idx < len(header_lower):
                        col_name = header_lower[idx]
                        if col_name in ['pv']:
                            pv = cell.strip()
                        elif col_name in ['type', 'typ']:
                            typ = cell.strip()
                        elif col_name in ['addr', 'address']:
                            addr = cell.strip()
                        elif col_name in ['size']:
                            size = cell.strip()
                        elif col_name in ['value', 'val']:
                            val = cell.strip()
                        elif col_name in ['note', 'notes', 'description', 'desc']:
                            note = cell.strip()
                        elif col_name in ['op', 'operation']:
                            # Skip operation column - not used in EPICS records
                            continue
                
                # If we couldn't map by header names, fall back to positional parsing
                if not pv and len(row) >= 1:
                    pv = row[0].strip()
                if not typ and len(row) >= 2:
                    typ = row[1].strip()
                
                # Handle different table formats by position if header mapping failed
                if len(row) == 7:  # PV | TYPE | OP | ADDR | SIZE | VALUE | NOTE
                    if not addr: addr = row[3].strip()
                    if not size: size = row[4].strip()
                    if not val: val = row[5].strip()
                    if not note: note = row[6].strip()
                elif len(row) == 6:  # PV | TYPE | OP | ADDR | VALUE | NOTE
                    if not addr: addr = row[3].strip()
                    if not val: val = row[4].strip()
                    if not note: note = row[5].strip()
                elif len(row) == 5:  # PV | TYPE | ADDR | VALUE | NOTE
                    if not addr: addr = row[2].strip()
                    if not val: val = row[3].strip()
                    if not note: note = row[4].strip()
                
                # Skip empty or invalid rows
                if pv and typ:
                    yield {"pv": pv, "type": typ, "addr": addr,
                           "size": size, "value": val, "note": note}
                
                i += 1
            continue
        i += 1

#---------------------------------------------------------------------------#

def generate_db(md_text: str) -> str:
    parts = [emit_common_header()]
    for ent in parse_tables(md_text):
        typ = ent["type"].lower()
        if typ in ("", "ndarray"):
            continue  # skip NDArray / empty
        if typ == "bo":
            parts.append(gen_bo(ent["pv"], ent["addr"], ent["value"], ent["note"]))
        elif typ == "bi":
            parts.append(gen_bi(ent["pv"], ent["addr"], ent["value"], ent["note"]))
        elif typ == "mbbo":
            parts.append(gen_mbbo(ent["pv"], ent["addr"], ent["value"], ent["note"]))
        elif typ == "mbbi":
            parts.append(gen_mbbi(ent["pv"], ent["addr"], ent["value"], ent["note"]))
        elif typ == "ao":
            parts.append(gen_numeric(typ, ent["pv"], ent["addr"], ent["note"], "OUT", "asynFloat64"))
        elif typ == "ai":
            parts.append(gen_numeric(typ, ent["pv"], ent["addr"], ent["note"], "INP", "asynFloat64"))
        elif typ == "longout":
            parts.append(gen_numeric(typ, ent["pv"], ent["addr"], ent["note"], "OUT", "asynInt32"))
        elif typ == "longin":
            parts.append(gen_numeric(typ, ent["pv"], ent["addr"], ent["note"], "INP", "asynInt32"))
        elif typ in ("stringin","stringout"):
            rectype, dtyp = map_record_type(typ)
            link = link_field_name(typ)
            parts.append(gen_numeric(rectype, ent["pv"], ent["addr"], ent["note"], link, dtyp))
        elif typ in ("waveform","waveformout","waveformin"):
            parts.append(gen_waveform(typ, ent["pv"], ent["addr"], ent["note"], ent["size"] or None))
        else:
            parts.append(f"/* Unsupported type '{ent['type']}' for PV {ent['pv']} */")
        parts.append("")
    return "\n".join(parts).rstrip() + "\n"#---------------------------------------------------------------------------#

def main():
    data = sys.stdin.read() if not sys.stdin.isatty() else (open(sys.argv[1], "r", encoding="utf-8").read() if len(sys.argv) > 1 else "")
    #sys.stdout.write(generate_db(data))
    with open(sys.argv[2], "w") as f:
        f.write(generate_db(data))

#---------------------------------------------------------------------------#

if __name__ == "__main__":
    main()
