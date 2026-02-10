#!/usr/bin/env python3

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]  # .../gemmini-rocc-tests
SRC = ROOT / "imagenet" / "resnet50.c"
OUT_DIR = Path(__file__).resolve().parent

CALL_START_RE = re.compile(r"tiled_matmul_nn_auto\s*\(")

TEMPLATE = """#include \"resnet_matmul_common.h\"

int main(void) {{
    resnet_matmul_setup();

    enum tiled_matmul_type_t tiled_matmul_type = OS;
    bool check = false;

    uint64_t start, end;
    uint64_t matmul_cycles = 0;

    start = read_cycles();

    uint64_t start_insn = read_instret();
    tiled_matmul_nn_auto({i}, {j}, {k},
        {input}, {weights}, {bias}, {output},
        {act}, {scale}, {repeating_bias},
        tiled_matmul_type, check, \"{label}\");
    uint64_t end_insn = read_instret();

    end = read_cycles();
    matmul_cycles += end - start;
    printf(\"matmul {label} cycles: %llu , insn: %llu\\n\",
        (unsigned long long)(end - start),
        (unsigned long long)(end_insn - start_insn));

    return 0;
}}
"""


def _strip_c_comments(s: str) -> str:
    # Remove /* ... */ first (including newlines), then // ...
    s = re.sub(r"/\*.*?\*/", "", s, flags=re.DOTALL)
    s = re.sub(r"//.*?$", "", s, flags=re.MULTILINE)
    return s


def _extract_call(text: str, start_idx: int) -> tuple[str, int]:
    """Return (call_text_including_semicolon, end_index_exclusive)."""
    i = start_idx
    # Find the first '(' after the function name
    open_paren = text.find("(", i)
    if open_paren == -1:
        raise ValueError("no '(' after call start")

    depth = 0
    in_str = False
    esc = False
    j = open_paren
    while j < len(text):
        ch = text[j]
        if in_str:
            if esc:
                esc = False
            elif ch == "\\":
                esc = True
            elif ch == '"':
                in_str = False
        else:
            if ch == '"':
                in_str = True
            elif ch == '(':
                depth += 1
            elif ch == ')':
                depth -= 1
                if depth == 0:
                    # Consume trailing whitespace then semicolon
                    k = j + 1
                    while k < len(text) and text[k].isspace():
                        k += 1
                    if k < len(text) and text[k] == ';':
                        return text[start_idx : k + 1], k + 1
        j += 1
    raise ValueError("unterminated call")


def _split_args(arg_str: str) -> list[str]:
    args: list[str] = []
    cur: list[str] = []
    depth = 0
    in_str = False
    esc = False

    for ch in arg_str:
        if in_str:
            cur.append(ch)
            if esc:
                esc = False
            elif ch == "\\":
                esc = True
            elif ch == '"':
                in_str = False
            continue

        if ch == '"':
            in_str = True
            cur.append(ch)
            continue

        if ch == '(':
            depth += 1
            cur.append(ch)
            continue
        if ch == ')':
            depth -= 1
            cur.append(ch)
            continue

        if ch == ',' and depth == 0:
            a = ''.join(cur).strip()
            if a:
                args.append(a)
            cur = []
            continue

        cur.append(ch)

    tail = ''.join(cur).strip()
    if tail:
        args.append(tail)
    return args


def main() -> int:
    text = SRC.read_text(encoding="utf-8", errors="replace")
    text = _strip_c_comments(text)

    seen_labels: set[str] = set()
    written = 0

    for m in CALL_START_RE.finditer(text):
        call_clean, _ = _extract_call(text, m.start())

        # Extract argument list content between the outermost parentheses.
        lpar = call_clean.find('(')
        rpar = call_clean.rfind(')')
        if lpar == -1 or rpar == -1 or rpar <= lpar:
            continue
        args_str = call_clean[lpar + 1 : rpar]
        args = _split_args(args_str)
        if len(args) < 13:
            continue

        # Expected signature (13 args): I, J, K, A, B, D, C, act, scale, repeating_bias, tiled_matmul_type, check, label
        label_arg = args[12].strip()
        if not (label_arg.startswith('"') and label_arg.endswith('"')):
            continue
        label = label_arg.strip('"')
        if label in seen_labels:
            continue
        seen_labels.add(label)

        out_name = f"{label}_matmul.c"
        out_path = OUT_DIR / out_name
        out_path.write_text(
            TEMPLATE.format(
                i=args[0],
                j=args[1],
                k=args[2],
                input=args[3],
                weights=args[4],
                bias=args[5],
                output=args[6],
                act=args[7],
                scale=args[8],
                repeating_bias=args[9],
                label=label,
            ),
            encoding="utf-8",
        )
        written += 1

    if written == 0:
        raise SystemExit(f"No tiled_matmul_nn_auto calls parsed in {SRC}")

    print(f"Generated {written} files into {OUT_DIR}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
