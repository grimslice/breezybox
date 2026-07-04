## compare_shells: bash mksh
## oils_failures_allowed: 2

#### space before ( in array initialization
# NOTE: mksh accepts this, but bash doesn't
a= (1 '2 3')
echo $a
## status: 2
## OK mksh status: 0
## OK mksh stdout: 1

#### array with invalid token
a=(
1
&
'2 3'
)
argv.py "${a[@]}"
## status: 2
## OK mksh status: 1

#### Arrays can't be used as env bindings
# Hm bash it treats it as a string!
A=a B=(b b) printenv.py A B
## status: 2
## stdout-json: ""
## OK bash STDOUT:
a
(b b)
## END
## OK bash status: 0
## OK mksh status: 1

#### Associative arrays can't be used as env bindings either
A=a B=([k]=v) printenv.py A B
## status: 2
## stdout-json: ""
## OK bash STDOUT:
a
([k]=v)
## OK bash status: 0
## OK mksh status: 1

#### Set array item to array
a=(1 2)
a[0]=(3 4)
echo "status=$?"
## stdout-json: ""
## status: 2
## OK mksh status: 1
## BUG bash stdout: status=1
## BUG bash status: 0

#### Multiple subscripts not allowed
# NOTE: bash 4.3 had a bug where it ignored the bad subscript, but now it is
# fixed.
a=('123' '456')
argv.py "${a[0]}" "${a[0][0]}"
## stdout-json: ""
## status: 2
## OK bash/mksh status: 1

#### Length op, index op, then transform op is not allowed
a=('123' '456')
echo "${#a[0]}" "${#a[0]/1/xxx}"
## stdout-json: ""
## status: 2
## OK bash/mksh status: 1

