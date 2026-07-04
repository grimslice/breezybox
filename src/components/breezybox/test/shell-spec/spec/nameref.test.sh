## oils_failures_allowed: 7
## compare_shells: bash mksh

#### named ref with $# doesn't work
set -- one two three

ref='#'
echo ref=$ref
typeset -n ref
echo ref=$ref

## STDOUT:
ref=#
ref=#
## END

# mksh does respect it!!  Gah.
## OK mksh STDOUT:
ref=#
ref=3
## END


#### assign to invalid ref
ref=1   # mksh makes this READ-ONLY!  Because it's not valid.

echo ref=$ref
typeset -n ref
echo ref=$ref

ref=foo
echo ref=$ref
## STDOUT:
ref=1
ref=1
ref=foo
## END
## OK mksh status: 2
## OK mksh STDOUT:
ref=1
ref=
## END

