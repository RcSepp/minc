// Source: http://llvm.org/docs/DebuggingJITedCode.html
//
// Commands:
// clang -g -O0 -S -emit-llvm showdebug.c // Generate showdebug.ll from showdebug.c
// llc -relocation-model=pic -o showdebug.o -filetype=obj showdebug.ll // Compile showdebug.ll to showdebug.o
// gcc -o showdebug showdebug.o // Link showdebug.o to ./showdebug
// gdb --quiet --args ./showdebug 5 // Run `./showdebug 5` with gdb
// (gdb) b main // Add breakpoint to main()
// (gdb) r // Run code
// (gdb) s // Step code
// (gdb) q // Quit

int compute_factorial(int n)
{
    if (n <= 1)
        return 1;

    int f = n;
    while (--n > 1)
        f *= n;
    return f;
}

int main(int argc, char** argv)
{
    if (argc < 2)
        return -1;
    char firstletter = argv[1][0];
    int result = compute_factorial(firstletter - '0');

    // Returned result is clipped at 255...
    return result;
}