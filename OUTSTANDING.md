OUTSTANDING
-----------


1. Is there a semantic reason for maintaining both blank and hole terminology? I prefer blank uniformly but can live with hole if the literature uses it canonically, but let's stick to one.

2. Let's consider targetting a 120-column formatting to allow for more longer lines to flow smoothly uninterrupted.

3. Does anything else merit the `_.get<...>()` treatment, from the std library verbs? And does anything merit `_.some_field_name` treatment (vs. a function)?

4. We can keep combinators in named form (compose, fanout, fanin, first, second, dup) in a sub-namespace, and make the prefered usage through sigil operators;
   some useful ones: like `&&&`, `***`, `+++`, `---`, `-->`, `++>`, `<++`, `<--`, `<<-`, `>>-`, `>>+`, `<<+`, `<<*`, `>>*`; one anomaly is the real operator `->.*`,
   because it could serve as compose, unless `->.*` as itself is actually useful. Then either we include the synthetic operators by default or make them a simple using away.   

5. The README needs cleaning up: some of the namespaces are incorrect (e.g. ranged `std::for_each`);
   some things should be used with inferred std types to reduce noise relative to tacit usage; and
   any new additions (like the synthetic operators, the two headers, etc).

6. Need to figure out the best way to split `_` and `$` into `_.hpp` and `$.hpp` with minimal repetition and ideally not needing a shared header (meaning no intra-includes) - potentially a build step produces the two exposed headers.
