## compare_shells: bash mksh zsh
## oils_failures_allowed: 0

# Test arithmetic expressions in all their different contexts.

# $(( 1 + 2 ))
# (( a=1+2 ))
# ${a[ 1 + 2 ]}
# ${a : 1+2 : 1+2}
# a[1 + 2]=foo

#### Empty expression a[]

a=(1 2 3)

a[]=42
echo status=$?
echo ${a[@]}

echo ${a[]}
echo status=$?

## status: 2
## STDOUT:
## END

## OK zsh status: 1

# runtime failures

## OK bash status: 0
## OK bash STDOUT:
status=1
1 2 3
status=1
## END

## BUG mksh status: 0
## BUG mksh STDOUT:
status=0
42 2 3
42
status=0
## END


# Others 
# [ 1+2 -eq 3 ]
# [[ 1+2 -eq 3 ]]
# unset a[]
# printf -v a[]

