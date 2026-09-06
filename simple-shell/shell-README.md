## Description
A simple interactive command-line shell written in C, supporting built-in commands and external program execution via `fork`/`execvp`.

## Why I Built It
- To understand how a shell actually works under the hood instead of just using one.
- To get hands-on practice with process creation (`fork`, `exec`, `wait`) after finishing K&R.
- To practice dynamic memory management in C (growable buffers with `realloc`).
- As a break project between K&R chapters, to apply what I'd learned to something real.

## How It Works
1. The shell prints a prompt and reads a line of input character by character, growing the input buffer dynamically with `realloc` as needed (no fixed line-length limit).
2. The line is tokenized on whitespace (`strtok` with a space/tab/newline delimiter set) into an argument array, also dynamically resized if a command has more tokens than the initial buffer allows.
3. If the first token matches a built-in command (`cd`, `help`, `exit`), it's dispatched directly through a function pointer table rather than spawning a process.
4. Otherwise, the shell forks: the child process replaces itself with the requested program via `execvp`, while the parent waits for it to exit or be signaled before returning to the prompt.
5. The loop continues until the user triggers `exit` or sends EOF (Ctrl+D).

## Known Limitations
- No support for pipes (`|`) or I/O redirection (`>`, `<`, `>>`).
- No background execution (`&`); every command blocks the shell until it finishes.
- The tokenizer splits purely on whitespace — no support for quoted arguments (e.g. `echo "hello world"` is split into two arguments) or escape characters.
- Only three built-ins are implemented (`cd`, `help`, `exit`); no `export`, `alias`, or environment variable expansion.
- No command history or line editing (arrow keys, backspace-across-realloc, etc.).
- No special handling of signals like Ctrl+C — it currently just affects the running child, not the shell's own loop.

## Key Notes
- Learned how `fork()` + `execvp()` + `waitpid()` fit together: the child replaces its own memory image via `exec`, so anything after a successful `execvp` call in the child never actually runs.
- Using an array of function pointers (`builtin_func[]`) alongside a matching array of command names (`builtin_str[]`) was a clean way to dispatch built-ins without a long `if/else` or `switch` chain.
- Writing `readline()` and `readtok()` with manual `realloc`-based growth was a good refresher on dynamic memory handling from K&R, applied to a real interactive program instead of an exercise.
- `WIFEXITED` / `WIFSIGNALED` matter because `waitpid` can return for reasons other than the child actually finishing (e.g. `WUNTRACED`), so checking these avoids treating a stopped-but-still-alive child as done.

## Usage
Compile with:
```
gcc -Wall -Wextra -Werror shell.c -o shell
```

Run it:
```
./shell
```

Example session:
```
My shell> ls
shell.c  shell  README.md
My shell> cd ..
My shell> help
MMOUCH's SHell PROGRAM
Try typing the program's names and hit ENTER
Here's builtin programs:
- cd
- help
- exit
My shell> exit
```

***HAVE FUN SHELLING***
