# rip

Arbitrary computation on x86 without using registers.  See [this article][] for
details.

Requirements: AMD64 Linux with `gcc`, `nasm`, and Python.

`compiler` takes Brainfuck source (not a filename) from the first command line
argument or from stdin.  It produces a binary named `rip`, which executes that
Brainfuck program without using registers (except for system calls).

Use `regclobber` to verify this by setting registers to zero after each
instruction.

~~~
$ gcc -O2 -Wall -o regclobber regclobber.c
$ ./regclobber ./rip
Hello World!

Executed 3268 instructions; clobbered registers 3187 times.
~~~

[this article]: http://mainisusuallyafunction.blogspot.com/2014/02/x86-is-turing-complete-with-no-registers.html
