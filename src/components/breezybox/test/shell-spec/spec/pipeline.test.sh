## oils_failures_allowed: 1
## compare_shells: bash dash mksh zsh

#
# Tests for pipelines.
# NOTE: Grammatically, ! is part of the pipeline:
#
# pipeline         :      pipe_sequence
#                  | Bang pipe_sequence

#### Brace group in pipeline
{ echo one; echo two; } | tac
## STDOUT:
two
one
## END

#### For loop starts pipeline
for w in one two; do
  echo $w
done | tac
## STDOUT:
two
one
## END

#### While Loop ends pipeline
seq 3 | while read i
do
  echo ".$i"
done
## STDOUT:
.1
.2
.3
## END

#### Redirect in Pipeline
echo hi 1>&2 | wc -l
## stdout: 0
## BUG zsh stdout: 1

#### Pipeline comments
echo abcd |    # input
               # blank line
tr a-z A-Z     # transform
## stdout: ABCD

#### Exit code is last status
echo a | egrep '[0-9]+'
## status: 1

#### ! turns non-zero into zero
! $SH -c 'exit 42'; echo $?
## stdout: 0
## status: 0

#### ! turns zero into 1
! $SH -c 'exit 0'; echo $?
## stdout: 1
## status: 0

#### ! in if
if ! echo hi; then
  echo TRUE
else
  echo FALSE
fi
## STDOUT:
hi
FALSE
## END
## status: 0

#### ! with ||
! echo hi || echo FAILED
## STDOUT:
hi
FAILED
## END
## status: 0

#### ! with { }
! { echo 1; echo 2; } || echo FAILED
## STDOUT:
1
2
FAILED
## END
## status: 0

#### ! with ( )
! ( echo 1; echo 2 ) || echo FAILED
## STDOUT:
1
2
FAILED
## END
## status: 0

#### ! is not a command
v='!'
$v echo hi
## status: 127

#### Nested pipelines
{ sleep 0.1 | seq 3; } | cat
{ sleep 0.1 | seq 10; } | { cat | cat; } | wc -l
## STDOUT:
1
2
3
10
## END

#### Pipeline in eval
ls /dev/null | eval 'cat | cat' | wc -l
## STDOUT:
1
## END


