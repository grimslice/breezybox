## compare_shells: dash bash mksh zsh
## oils_failures_allowed: 1

# Test the length oeprator, which dash supports.  Dash doesn't support most
# other ops.

#### String length
v=foo
echo ${#v}
## stdout: 3

#### Length of undefined variable
echo ${#undef}
## stdout: 0

#### Length of undefined variable with nounset
set -o nounset
echo ${#undef}
## status: 1
## OK dash status: 2

