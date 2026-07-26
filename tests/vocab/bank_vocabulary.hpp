// A vocabulary file: entries only, no include guard — it is expanded once per tacit surface.
TACIT_VERB(deposit)                 // x.deposit(a...)
TACIT_VERB(balance)                 // x.balance()
TACIT_FREE(risk, bank::risk)        // bank::risk(x)   — free function, unreachable via a verb list
TACIT_CPO(tier, bank::tier)         // bank::tier(x)   — a customization point
TACIT_NOUN(money_type)              // _::money_type::of<X> == X::money_type
