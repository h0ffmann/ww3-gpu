# PTC, Vale style

The mechanical part of Português Técnico Controlado (PTC), a skill by Kayque Rotondo that rewrites
pt-BR technical text so that each sentence has one reading
(<https://github.com/kayquer/portugues-tecnico-controlado>, `SKILL.md` v1.2.0, commit `8938f58`,
read 2026-11-11). The rule names and word lists here are derived from its `SKILL.md`,
`references/lexico.md` and `references/ruido-ia.md`. `scripts/vale.sh --ptc` runs them, as
warnings and suggestions that never fail CI.

A regex cannot judge PTC-1's subject in every clause, PTC-2's one proposition per sentence and
explicative relative, PTC-3's vague quantifiers, PTC-5's one subordinate clause and chain of
`de`, PTC-6's one term per concept, or the levels (`estrito`, `descritivo`, `leve`). Those stay
with the skill and a person.

The upstream licence, which covers the derived lists:

> MIT License
>
> Copyright (c) 2026 Kayque Rotondo
>
> Permission is hereby granted, free of charge, to any person obtaining a copy of this software
> and associated documentation files (the "Software"), to deal in the Software without
> restriction, including without limitation the rights to use, copy, modify, merge, publish,
> distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
> Software is furnished to do so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in all copies or
> substantial portions of the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
> BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
> NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
> DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
> OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
