/* A char function's result used as a condition (chapter 11).
 * The callee returns its value in B only; the test must look at B. */
char next();

skip()
{
    if (next())
        return 1;
    return 0;
}
