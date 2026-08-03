// Include order 4 of 6: $ → λ → _. One of a complete set — a TU has exactly one
// include order, so order-independence can only be shown by having one TU per permutation. The
// assertions live in `order_body.hpp`, shared by all six, so the ONLY variable across the set is
// the order itself. See order_cdl.cpp for what is actually at stake.
// clang-format off
#include <tacit/$.hpp>
#include <tacit/λ.hpp>
#include <tacit/_.hpp>
// clang-format on

#include "order_body.hpp"

int main() { return order_body(); }
