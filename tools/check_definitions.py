#!/usr/bin/env python3
"""Find member functions declared in a header with no definition anywhere.

This exists because the same mistake happened twice, both times mine and
both times invisible until the link step: a scripted edit replaced a
region of a .cpp and took the tail of it with it, deleting whole
function bodies. Every translation unit still compiled — a definition
that is simply absent is not a compile error — so the first sign of
trouble was a wall of "Undefined symbols for architecture arm64" after
a hundred files had been rebuilt.

The linker does catch it. It catches it at the end of a five-minute
build, on the operator's machine, which is the worst place to find out.
This runs in a second.

Deliberately conservative: it looks for the definition anywhere under
src/, not just in the sibling .cpp, and it skips everything whose
definition would not be expected in a .cpp at all — signals (moc writes
those), pure virtuals, = default / = delete, and anything with an inline
body in the header. False negatives are fine here; a false positive
would train people to ignore the output.
"""

import os
import re
import sys

# Names that fall out of the regex when it meets something that is not
# a declaration at all — a static_assert, a macro call, a lambda.
KEYWORDS = {
    'return', 'if', 'for', 'while', 'switch', 'sizeof', 'void', 'bool',
    'int', 'float', 'double', 'char', 'auto', 'const', 'static_assert',
    'explicit', 'operator', 'QStringLiteral', 'Q_DECLARE_METATYPE',
    'decltype', 'noexcept', 'alignas',
}

SKIP_BODY = re.compile(r'=\s*(0|default|delete)\s*;')
DECL = re.compile(
    r'^\s*(?:\[\[[^\]]*\]\]\s*)?'
    r'(?:static\s+|virtual\s+|explicit\s+|constexpr\s+|inline\s+|friend\s+)*'
    r'(?:[\w:]+(?:\s*<[^;]*>)?[\s\*&]+)?'      # return type, optional (ctors)
    r'(~?\w+)\s*\('                             # name
)


def class_spans(text):
    """Yield (class_name, body_text) for each class/struct with a body."""
    for m in re.finditer(r'\b(?:class|struct)\s+(\w+)\s*(?::[^{;]*)?\{', text):
        name = m.group(1)
        i = m.end() - 1
        depth = 0
        for j in range(i, len(text)):
            if text[j] == '{':
                depth += 1
            elif text[j] == '}':
                depth -= 1
                if depth == 0:
                    yield name, text[i + 1:j]
                    break


def strip_comments(text):
    """Remove comments, without being fooled by string literals.

    The regex version of this was wrong in a way worth recording: a
    `/*` inside a string literal — and this codebase has several, in
    URLs and in format strings — makes a non-greedy `/\*.*?\*/` eat
    everything up to the next real close comment, hundreds of lines
    away. The scan then reports every function in the swallowed region
    as undefined. A checker that cries wolf is worse than no checker, so
    this walks the text instead.
    """
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c in '"\'':
            quote = c
            out.append(c)
            i += 1
            while i < n:
                out.append(text[i])
                if text[i] == '\\':
                    i += 2
                    if i - 1 < n:
                        out.append(text[i - 1])
                    continue
                if text[i] == quote:
                    i += 1
                    break
                i += 1
            continue
        if c == '/' and i + 1 < n and text[i + 1] == '/':
            while i < n and text[i] != '\n':
                i += 1
            continue
        if c == '/' and i + 1 < n and text[i + 1] == '*':
            i += 2
            while i + 1 < n and not (text[i] == '*' and text[i + 1] == '/'):
                i += 1
            i += 2
            out.append(' ')
            continue
        out.append(c)
        i += 1
    return ''.join(out)


def declarations(body):
    """Names declared in `body` that a .cpp is expected to define."""
    out = []
    access = 'private'
    depth = 0
    for raw in body.split('\n'):
        line = raw.strip()
        m = re.match(r'^(public|protected|private)?\s*(signals|slots)?\s*:$', line)
        if m and (m.group(1) or m.group(2)):
            access = 'signals' if m.group(2) == 'signals' else 'normal'
            continue
        # Skip nested class bodies and inline function bodies.
        depth += line.count('{') - line.count('}')
        if depth > 0 or '{' in line:
            continue
        if access == 'signals':
            continue
        if not line.endswith(';') or '(' not in line:
            continue
        if SKIP_BODY.search(line):
            continue
        if line.startswith(('Q_', 'using ', 'typedef ', 'friend ')):
            continue
        d = DECL.match(line)
        if not d:
            continue
        name = d.group(1)
        if name in KEYWORDS:
            continue
        # A member variable of function-pointer or functor type.
        if re.match(r'^\s*(std::function|QPointer|std::unique_ptr)', line):
            continue
        out.append(name)
    return out


def main(root='src'):
    corpus = {}
    for dirpath, _, names in os.walk(root):
        for n in names:
            if n.endswith(('.cpp', '.mm')):
                p = os.path.join(dirpath, n)
                corpus[p] = strip_comments(open(p, encoding='utf-8').read())
    # One pass over every .cpp collects `Class::name` as a set, rather
    # than running a regex per declaration over the whole corpus — the
    # difference between a second and several minutes.
    defined = set()
    macro_defined = set()
    for text in corpus.values():
        for m in re.finditer(r'\b(\w+)::(~?\w+)\s*\(', text):
            defined.add((m.group(1), m.group(2)))
        # Setters written by a macro — Hl2OptionsModel has nine of them —
        # never appear as `Class::name(` anywhere. Take the first
        # argument of any SHOUTING_MACRO call as a name that might have
        # been defined that way, and stay quiet about it.
        for m in re.finditer(r'\b[A-Z][A-Z0-9_]{3,}\s*\(\s*(\w+)\s*[,)]', text):
            macro_defined.add(m.group(1))

    missing = []
    for dirpath, _, names in os.walk(root):
        for n in names:
            if not n.endswith('.h'):
                continue
            path = os.path.join(dirpath, n)
            text = strip_comments(open(path, encoding='utf-8').read())
            for cls, body in class_spans(text):
                for name in declarations(body):
                    if (cls, name) not in defined \
                            and name not in macro_defined:
                        missing.append((path, cls, name))

    for path, cls, name in missing:
        print('%s: %s::%s declared but never defined' % (path, cls, name))
    print('%d declaration(s) with no definition' % len(missing))
    return 1 if missing else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else 'src'))
