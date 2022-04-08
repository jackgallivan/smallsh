# Script to compile smallsh for assignment 3.

gcc -std=c11 -Wall -Werror -g3 -O0 smallsh.c smallsh_funcs.c -o smallsh
