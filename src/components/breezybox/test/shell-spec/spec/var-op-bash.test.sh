## compare_shells: bash
## oils_failures_allowed: 7

#### ${!var[@]@X}
# note: "y z" causes a bug!
$SH -c 'declare -A A=(["x"]="y"); echo ${!A[@]@P}'
if test $? -ne 0; then echo fail; fi

# note: "y z" causes a bug!
$SH -c 'declare -A A=(["x y"]="y"); echo ${!A[@]@Q}'
if test $? -ne 0; then echo fail; fi

$SH -c 'declare -A A=(["x"]=y); echo ${!A[@]@a}'
if test $? -ne 0; then echo fail; fi
# STDOUT:



# END

#### ${#var@X} is a parse error
# note: "y z" causes a bug!
$SH -c 'declare -A A=(["x"]="y"); echo ${#A[@]@P}'
if test $? -ne 0; then echo fail; fi

# note: "y z" causes a bug!
$SH -c 'declare -A A=(["x"]="y"); echo ${#A[@]@Q}'
if test $? -ne 0; then echo fail; fi

$SH -c 'declare -A A=(["x"]=y); echo ${#A[@]@a}'
if test $? -ne 0; then echo fail; fi
## STDOUT:
fail
fail
fail
## END

