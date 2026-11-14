"""Check bilingual coverage and placeholder parity. Run after adding/editing visible text."""
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[1] / "Source/Game_Jam_Clean_Vers"
literal = r'"((?:\\.|[^"\\])*)"'
catalog = root / "Localization/GameLanguageCatalog.inl"
pairs = re.findall(r'PAIR\(' + literal + r',\s*' + literal + r'\)', catalog.read_text(encoding="utf-8"))
known = {s for pair in pairs for s in pair}
errors = []
for zh, en in pairs:
    fmt = r'%(?:[0-9.+-]*[sdfg]|%)'
    if re.findall(fmt, zh) != re.findall(fmt, en):
        errors.append(f"Placeholder mismatch: {zh} -> {en}")
    if not zh or not en:
        errors.append(f"Empty translation: {zh} -> {en}")
for path in root.rglob("*"):
    if path.suffix not in (".h", ".cpp") or any(x in str(path) for x in ("Tests", "Tools", "Authoring", "Localization")):
        continue
    source = path.read_text(encoding="utf-8-sig")
    # Chinese literals and English-authored NPC lines must both be cataloged.
    for text in re.findall(r'TEXT\(' + literal + r'\)', source):
        if re.search(r'[\u4e00-\u9fff]', text) and text not in known:
            errors.append(f"Missing pair: {path.relative_to(root)}: {text}")
    for text in re.findall(r'Say\(TEXT\(' + literal + r'\)', source):
        if text not in known:
            errors.append(f"Missing NPC pair: {path.relative_to(root)}: {text}")
print(f"Bilingual entries: {len(pairs)}; errors: {len(errors)}")
for error in errors:
    print(error)
sys.exit(bool(errors))
