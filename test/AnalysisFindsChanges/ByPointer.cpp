// RUN: %clang_cc1 %change %s -fsyntax-only -verify

void unary_operators() {
    int i = 0;
    int const * iptr = &i;

    ++iptr; // expected-note {{variable 'iptr' was changed}}
    --iptr; // expected-note {{variable 'iptr' was changed}}
    iptr++; // expected-note {{variable 'iptr' was changed}}
    iptr--; // expected-note {{variable 'iptr' was changed}}
}

void binary_operators() {
    int i = 0;
    int * const iptr = &i;

    *iptr = 1; // expected-note {{variable 'iptr' was changed}}
    *iptr *= 1; // expected-note {{variable 'iptr' was changed}}
    *iptr /= 1; // expected-note {{variable 'iptr' was changed}}
    *iptr %= 2; // expected-note {{variable 'iptr' was changed}}
    *iptr += 2; // expected-note {{variable 'iptr' was changed}}
    *iptr -= 2; // expected-note {{variable 'iptr' was changed}}
    *iptr <<= 1; // expected-note {{variable 'iptr' was changed}}
    *iptr >>= 1; // expected-note {{variable 'iptr' was changed}}
    *iptr |= 2; // expected-note {{variable 'iptr' was changed}}
    *iptr ^= 2; // expected-note {{variable 'iptr' was changed}}
    *iptr &= 1; // expected-note {{variable 'iptr' was changed}}
}