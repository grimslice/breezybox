## compare_shells: bash mksh
## oils_failures_allowed: 2

# Arrays decay upon assignment (without splicing) and equality.

#### Assignment Causes Array Decay
set -- x y z
argv.py "[$@]"
var="[$@]"
argv.py "$var"
## STDOUT:
['[x', 'y', 'z]']
['[x y z]']
## END

