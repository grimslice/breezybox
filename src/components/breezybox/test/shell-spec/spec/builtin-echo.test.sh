## oils_failures_allowed: 0
## compare_shells: dash bash mksh zsh ash

# TODO mapfile options: -c, -C, -u, etc.

#### echo dashes
echo -
echo --
echo ---
## STDOUT:
-
--
---
## END
## BUG zsh STDOUT:

--
---
## END

#### echo backslashes
echo \\
echo '\'
echo '\\'
echo "\\"
## STDOUT:
\
\
\\
\
## BUG dash/mksh/zsh STDOUT:
\
\
\
\
## END

#### echo builtin should disallow typed args - literal
echo (42)
## status: 2
## OK mksh/zsh status: 1
## STDOUT:
## END

#### echo builtin should disallow typed args - variable
var x = 43
echo (x)
## status: 2
## OK mksh/zsh status: 1
## STDOUT:
## END

#### echo -ez (invalid flag)
# bash differs from the other three shells, but its behavior is possibly more
# sensible, if you're going to ignore the error.  It doesn't make sense for
# the 'e' to mean 2 different things simultaneously: flag and literal to be
# printed.
echo -ez 'abc\n'
## STDOUT:
-ez abc\n
## END
## OK dash/mksh/zsh STDOUT:
-ez abc

## END

#### echo to redirected directory is an error
mkdir -p dir

echo foo > ./dir
echo status=$?
printf foo > ./dir
echo status=$?

## STDOUT:
status=1
status=1
## END
## OK dash STDOUT:
status=2
status=2
## END

