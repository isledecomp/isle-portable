// atof.c
#include <ctype.h>

// A lightweight atof alternative
double atof(const char *s) {
    double result = 0.0;
    int sign = 1;
    double fraction = 0.0;
    int divisor = 1;

    // Skip whitespace
    while (*s == ' ' || *s == '\t') s++;

    // Handle sign
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }

    // Integer part
    while (*s >= '0' && *s <= '9') {
        result = result * 10.0 + (*s - '0');
        s++;
    }

    // Fractional part
    if (*s == '.') {
        s++;
        while (*s >= '0' && *s <= '9') {
            fraction = fraction * 10.0 + (*s - '0');
            divisor *= 10;
            s++;
        }
        result += fraction / divisor;
    }

    return sign * result;
}
