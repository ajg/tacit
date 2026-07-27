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

4. ~~Synthetic sigil operators.~~ **DONE (opt-in: `TACIT_COMBINATORIAL_OPERATORS`)** — `->*` compose
   left-to-right, `<<*` compose right-to-left, `&&&` fanout, `***` product. Exhaustive sweep in
   `tacit_extras.md`: 7194 candidates, 615 stolen by maximal munch, 368 of the 391 practical ones
   compile. Haskell's `&&&`/`***`/`+++` survive C++'s lexer; `|||`/`>>>`/`<<<` do not, which is why
   compose is `->*` (a real, single, unclaimed operator). `->.*` is not a token sequence at all —
   `->` needs an id-expression after it. The named forms (compose/fanout/first/second) already exist
   behind `TACIT_COMBINATORS`; moving them to a sub-namespace is still open.

5. README. **Namespaces fixed** — every range-taking algorithm is now `std::ranges::`; the headline
   example did not compile before. Still outstanding: inferred std types to cut noise, and documenting
   whatever lands from #4 and #6.

6. Need to figure out the best way to split `_` and `$` into `_.hpp` and `$.hpp` with minimal repetition and ideally not needing a shared header (meaning no intra-includes) - potentially a build step produces the two exposed headers.
