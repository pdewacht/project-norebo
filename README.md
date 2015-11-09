# Project Norebo

Norebo is a hack to run some _Project Oberon 2013_ software on the
Unix command line. Programs that use the GUI obviously won't work, but
e.g. the compiler runs.

I probably won't be maintaining this project, so feel free to fork
if you want to develop it further.

## Contents

* `Runtime/` RISC5 emulator and operating system interface.
* `Oberon/` Unmodified source code from Project Oberon 2013.
* `Norebo/` Customized Norebo modules.
* `Bootstrap/` Pre-compiled modules to bootstrap Norebo.
* `build.sh` The build script. See Norebo in action.

## File handling

New files are always created in the current directory. Old files are
first looked up in the current directory and if they are not found,
they are searched for in the path defined by the OBERON_PATH
environment variable. Files found via OBERON_PATH are always opened
read-only.

## Bugs

Probably many.

Files are not integrated with the garbage collector. If you don't
close a file, it will remain open until Norebo exits.

Most runtime errors do not print a diagnostic message. Here's a table
of exit codes:

 Exit code | Meaning
----------:|:------------------------------
      1..7 | possibly a Modules error
         5 | (also) unknown command
       101 | array index out of range
       102 | type guard failure
       103 | array or string copy overflow
       104 | access via NIL pointer
       105 | illegal procedure call
       106 | integer division by zero
       107 | assertion violated
