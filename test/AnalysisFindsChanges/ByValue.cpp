// RUN: %clang_cc1 %change %s -fsyntax-only -verify

void unary_operators() {
    int i = 0;

    ++i; // expected-note {{variable 'i' was changed}}
    --i; // expected-note {{variable 'i' was changed}}
    i++; // expected-note {{variable 'i' was changed}}
    i--; // expected-note {{variable 'i' was changed}}
}

void binary_operators() {
    int i = 0;

    i = 1; // expected-note {{variable 'i' was changed}}
    i *= 1; // expected-note {{variable 'i' was changed}}
    i /= 1; // expected-note {{variable 'i' was changed}}
    i %= 2; // expected-note {{variable 'i' was changed}}
    i += 2; // expected-note {{variable 'i' was changed}}
    i -= 2; // expected-note {{variable 'i' was changed}}
    i <<= 1; // expected-note {{variable 'i' was changed}}
    i >>= 1; // expected-note {{variable 'i' was changed}}
    i |= 2; // expected-note {{variable 'i' was changed}}
    i ^= 2; // expected-note {{variable 'i' was changed}}
    i &= 1; // expected-note {{variable 'i' was changed}}

    int j = 0;
    i = // expected-note {{variable 'i' was changed}}
        ++j; // expected-note {{variable 'j' was changed}}
}

void change_in_for_loop() {
    for ( int i = 0
        ; i < 10
        ; ++i ) // expected-note {{variable 'i' was changed}}
    ;
}

void declaration_statements() {
    int i, j;
    i = j; // expected-note {{variable 'i' was changed}}
}