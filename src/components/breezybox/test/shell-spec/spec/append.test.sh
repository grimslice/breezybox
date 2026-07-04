# spec/append.test.sh: Test +=

## compare_shells: bash mksh zsh

#### Try to append list to element
# bash - runtime error: cannot assign list to array number
# mksh - a[-1]+: is not an identifier
# osh - parse error -- could be better!
a=(1 '2 3')
a[-1]+=(4 5)
argv.py "${a[@]}"

## stdout-json: ""
## status: 2

## OK bash status: 0
## OK bash STDOUT:
['1', '2 3']
## END

## OK zsh status: 0
## OK zsh STDOUT:
['1', '2 3', '4', '5']
## END

## OK mksh status: 1

