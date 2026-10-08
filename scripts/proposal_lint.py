#!/usr/bin/env python3
"""proposal_lint — deterministic checks on pubs/proposal that a model reviewer misses or invents.

    scripts/proposal_lint.py              # errors exit 1, warnings are printed and exit 0
    scripts/proposal_lint.py --strict     # warnings fail too

Errors:
  parity       each pt/NN-*.md and en/NN-*.md pair cites the same @keys and carries the same
               numbers (decimal comma folded to a point), so the two languages say the same thing;
  placeholder  (REFERÊNCIA), (CITAR ...), (PEGAR ...), TODO, XXX left in the text;
  decimal      a decimal point in Portuguese prose ("1.8 m" where ABNT wants "1,8 m").
Warnings:
  acronym      first use of an acronym, in reading order, without its expansion next to it
               ("Nome Longo (SIGLA)" or "SIGLA (nome longo)"), unless the DEL reader knows it;
  register     first person ("nós", "nosso") in the Portuguese text.

Ideas from Imbad0202/academic-research-skills (token conservation, acronym first use); the code is
written here. Standard library only.
"""
import argparse
import collections
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
PROPOSAL = ROOT / "pubs" / "proposal"

# Acronyms an electronics/computer-engineering committee reads without an expansion.
# Product and tool names (OpenMP, NetCDF) are names, not acronyms to expand; CNPq, MCTI and Finep
# are the Brazilian funding agencies every Poli reader has met.
KNOWN = {"GPU", "GPUs", "CPU", "MPI", "API", "UTC", "AMD", "NVIDIA", "CUDA", "HIP", "SYCL", "UFRJ",
         "DEL", "ABNT", "IEEE", "II", "III", "IV", "L1", "L2", "L3", "L4", "H100", "CI", "GNU",
         "WAVEWATCH", "OpenMP", "OpenACC", "GoogleTest", "CTest", "CMake", "NetCDF", "CNPq", "MCTI",
         "PCIe", "NVLink"}

CITE = re.compile(r"-?@([\w:.-]*\w)")
NUMBER = re.compile(r"(?<![\w.,])\d+(?:[.,]\d+)?(?![\w])")
ACRONYM = re.compile(r"(?<![\w/@-])([A-Z][A-Za-z]*[A-Z][A-Za-z0-9]*|[A-Z]{2,}\d*)(?![\w-])")
PLACEHOLDER = re.compile(r"(?i:\((?:REFER[ÊE]NCIA|CITAR|PEGAR)[^)]*\))|\bTODO\b|\bXXX\b")  # "Todo o" is Portuguese
PT_DECIMAL = re.compile(r"(?<![\w.])\d+\.\d+(?![\w.])")
FIRST_PERSON = re.compile(r"\b(nosso|nossa|nossos|nossas)\b", re.I)  # not "nós": it also means compute nodes


def prose(text: str) -> str:
    """The text a reader sees: no HTML comments, code spans, citations or list numbering."""
    text = re.sub(r"<!--.*?-->", "", text, flags=re.S)
    text = re.sub(r"```.*?```", "", text, flags=re.S)
    text = re.sub(r"`[^`]*`", "", text)
    text = re.sub(r"^\s*\d+\.\s", "", text, flags=re.M)
    return text.replace("*", "")


def numbers(text: str) -> collections.Counter:
    text = CITE.sub("", prose(text))
    return collections.Counter(n.replace(",", ".") for n in NUMBER.findall(text))


def parity(pt: pathlib.Path, en: pathlib.Path) -> list:
    a, b = pt.read_text(encoding="utf-8"), en.read_text(encoding="utf-8")
    out = []
    ka, kb = set(CITE.findall(prose(a))), set(CITE.findall(prose(b)))
    if ka != kb:
        out.append(f"parity {pt.name}: citations only in pt {sorted(ka - kb)}, only in en {sorted(kb - ka)}")
    na, nb = numbers(a), numbers(b)
    if na != nb:
        out.append(f"parity {pt.name}: numbers only in pt {sorted((na - nb).elements())}, "
                   f"only in en {sorted((nb - na).elements())}")
    return out


def defined_at(text: str, start: int, end: int) -> bool:
    """"Nome Longo (SIGLA)", "SIGLA (nome longo)" or "SIGLA, um ..." (apposition)."""
    return (text[max(0, start - 1):start] == "("
            or re.match(r" \(|, (um|uma|o|a|an?|the) ", text[end:end + 7]) is not None)


def check(base: pathlib.Path = PROPOSAL) -> tuple:
    errors, warnings = [], []
    pts = sorted(base.glob("pt/[0-9][0-9]-*.md"))
    for pt in pts:
        en = base / "en" / pt.name
        if not en.exists():
            errors.append(f"parity {pt.name}: no en/{pt.name}")
            continue
        errors += parity(pt, en)
    seen = set()
    for f in pts + sorted(base.glob("en/[0-9][0-9]-*.md")):
        text = prose(f.read_text(encoding="utf-8"))
        rel = f.relative_to(base).as_posix()
        for m in PLACEHOLDER.finditer(text):
            errors.append(f"placeholder {rel}: {m.group(0)!r}")
        if f.parent.name != "pt":
            continue
        uncited = CITE.sub("", text)
        for m in PT_DECIMAL.finditer(uncited):
            ctx = uncited[max(0, m.start() - 20):m.start()]
            if not re.search(r"(versão|revisão|develop|v)\s*$", ctx):
                errors.append(f"decimal {rel}: {m.group(0)!r} (use vírgula decimal)")
        for m in FIRST_PERSON.finditer(text):
            warnings.append(f"register {rel}: {m.group(0)!r}")
        body = CITE.sub("", re.sub(r"^#.*$", "", text, flags=re.M))
        for m in ACRONYM.finditer(body):
            word = m.group(1)
            if word in seen or word in KNOWN:
                continue
            seen.add(word)
            if not defined_at(body, m.start(), m.end()):
                warnings.append(f"acronym {rel}: {word} used before it is expanded")
    return errors, warnings


def self_test() -> None:
    import tempfile
    base = pathlib.Path(tempfile.mkdtemp())
    (base / "pt").mkdir()
    (base / "en").mkdir()
    (base / "pt" / "03-theme.md").write_text(
        "<!-- 2026-10-01 -->\n# TEMA\n\nO Centro Nacional (CN) mede 1,8 m [@a2020] na revisão `x` 7.14."
        " Todo o código roda em nós. O ABC, um modelo, e o XYZ rodam.\n")
    (base / "en" / "03-theme.md").write_text(
        "# THEME\n\nThe National Centre (CN) measures 1.8 m [@a2020] at revision `x` 7.14.\n")
    errors, warnings = check(base)
    assert errors == [], errors
    assert warnings == ["acronym pt/03-theme.md: XYZ used before it is expanded"], warnings
    (base / "en" / "03-theme.md").write_text("# THEME\n\nThe centre measures 1.3 m [@b2021]. TODO\n")
    (base / "pt" / "03-theme.md").write_text("# TEMA\n\nMede 1.8 m (CITAR ALGO) [@a2020].\n")
    errors, _ = check(base)
    kinds = sorted(e.split()[0] for e in errors)
    assert kinds == ["decimal", "parity", "parity", "placeholder", "placeholder"], errors
    print("proposal_lint self-test ok")


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--strict", action="store_true", help="warnings fail too")
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args(argv)
    if a.self_test:
        self_test()
        return 0
    errors, warnings = check()
    for e in errors:
        print(f"error: {e}", file=sys.stderr)
    for w in warnings:
        print(f"warning: {w}", file=sys.stderr)
    if not errors and not warnings:
        print("proposal_lint: clean")
    return 1 if errors or (a.strict and warnings) else 0


if __name__ == "__main__":
    sys.exit(main())
