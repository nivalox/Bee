/* types.c - Bee's value types and the assignment-compatibility rules */
#include "bee.h"

const char *tyname(int ty) {
    switch (ty) {
    case TY_INT: return "an int";
    case TY_CHAR: return "a char";
    case TY_FLOAT: return "a float";
    case TY_STR: return "a string";
    default: return "an untyped word";
    }
}

/* May a value of type src be stored in a variable of type dst?
 * Untyped B words are never restricted. An int widens to a float (the value is converted),
 * but nothing narrows or changes kind silently. */
const char *assign_err(int dst, int src) {
    if (dst == TY_WORD) return NULL;
    switch (dst) {
    case TY_INT:
    case TY_CHAR:
        if (src == TY_FLOAT) return "a float is never converted to an integer implicitly";
        if (src == TY_STR) return "a string is not a number";
        return NULL;
    case TY_FLOAT:
        if (src == TY_STR) return "a string is not a number";
        if (src == TY_WORD) return "an untyped B word is not known to hold a float (use a float expression)";
        return NULL;
    case TY_STR:
        if (src == TY_STR || src == TY_WORD) return NULL;
        return "only a string (or an untyped word holding a string address) can be stored in a string";
    }
    return NULL;
}
