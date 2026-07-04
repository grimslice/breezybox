## oils_failures_allowed: 0
## compare_shells: dash bash mksh zsh

#### time pipeline
time echo hi | wc -c
## stdout: 3
## status: 0

#### shift
set -- 1 2 3 4
shift
echo "$@"
shift 2
echo "$@"
## STDOUT:
2 3 4
4
## END
## status: 0

#### Shifting too far
set -- 1
shift 2
## status: 1
## OK dash status: 2

#### Invalid shift argument
shift ZZZ
## status: 2
## OK bash status: 1
## BUG mksh/zsh status: 0
