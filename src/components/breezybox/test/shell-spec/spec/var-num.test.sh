# Test $0 $1 $2

## tags: interactive
## compare_shells: bash dash mksh

# ignored comment

#### In function
myfunc() {
  echo $1 ${2}
}
myfunc a b c d
## stdout: a b

