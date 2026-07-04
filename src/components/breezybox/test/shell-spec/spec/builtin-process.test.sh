## compare_shells: dash bash mksh zsh
## oils_failures_allowed: 1
## oils_cpp_failures_allowed: 2
# case #24 with ulimit -f 1 is different under C++ for some reason - could be due to the python2
# intepreter and SIGXFSZ

#### Exit out of function
f() { exit 3; }
f
exit 4
## status: 3

#### Exit builtin with invalid arg 
exit invalid
# Rationale: runtime errors are 1
## status: 1
## OK dash/bash status: 2
## BUG zsh status: 0

#### Exit builtin with too many args
# This is a parse error in OSH.
exit 7 8 9
echo status=$?
## status: 2
## stdout-json: ""
## BUG bash/zsh status: 0
## BUG bash/zsh stdout: status=1
## BUG dash status: 7
## BUG dash stdout-json: ""
## OK mksh status: 1
## OK mksh stdout-json: ""
