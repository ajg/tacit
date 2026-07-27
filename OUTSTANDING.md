OUTSTANDING
-----------


1. ~~blank vs hole terminology.~~ **DONE** — `blank` everywhere; `hole<A...>` is now `blank<A...>`,
   `TACIT_STD_HOLES` is `TACIT_STD_BLANKS`, and the header states the settled vocabulary: `_` is the
   *placeholder* (the standard's own word, already used by the `is_tacit_placeholder` tag), a *blank*
   is the gap it leaves, at term or type level.

2. Let's consider targetting a 120-column formatting to allow for more longer lines to flow smoothly uninterrupted.

3. ~~`_.get<...>()` treatment; field-style names.~~ **DONE** — added `to<C>` (ranges::to, both kinds
   of argument), `any_cast`, `holds_alternative`, `duration_cast`, and the three pointer casts, all
   reached by ADL so the header gains no includes. Field-style `_.first` / `_.second` added.
   Deliberately NOT added: chrono `floor`/`ceil`/`round` (`<cmath>`'s `floor` is visible, so
   `floor<seconds>(x)` misbehaves — `duration_cast` covers it); `span::first<N>`/`last<N>` (would
   collide with the field-style names, and the runtime `_.subspan(o, n)` already covers it);
   `variant::emplace<T>(a...)` (would be ambiguous with the existing runtime `emplace`).

4. We can keep combinators in named form (compose, fanout, fanin, first, second, dup) in a sub-namespace, and make the prefered usage through sigil operators;
   some useful ones: like `&&&`, `***`, `+++`, `---`, `-->`, `++>`, `<++`, `<--`, `<<-`, `>>-`, `>>+`, `<<+`, `<<*`, `>>*`; one anomaly is the real operator `->.*`,
   because it could serve as compose, unless `->.*` as itself is actually useful. Then either we include the synthetic operators by default or make them a simple using away.   

5. README. **Namespaces fixed** — every range-taking algorithm is now `std::ranges::`; the headline
   example did not compile before. Still outstanding: inferred std types to cut noise, and documenting
   whatever lands from #4 and #6.

6. Need to figure out the best way to split `_` and `$` into `_.hpp` and `$.hpp` with minimal repetition and ideally not needing a shared header (meaning no intra-includes) - potentially a build step produces the two exposed headers.
