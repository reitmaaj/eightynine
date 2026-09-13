/* Intentionally NOT valid C89: uses a // comment. */
int probe_comment(void)
{
    return 1; // C99 line comment
}

/* Intentionally NOT valid C89: declaration after a statement. */
int probe_decl(void)
{
    int a = 1;
    int b = 2;
    return a + b;
}
