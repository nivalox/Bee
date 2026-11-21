// whole-line comment
/* block comment */
main() { // trailing comment
    /* multi
       line */
    int x = 1; // set x
    printf("%d // not a comment*n", x);
    printf("a/**b**/c*n");   // comment-like text inside a string (** is a literal star)
    x = 8 // 2 is not division
        ;
    printf("%d*n", x);
}
