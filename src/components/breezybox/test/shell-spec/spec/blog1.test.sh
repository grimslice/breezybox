## compare_shells: dash bash mksh zsh

# Tests for the blog.
#
# Fun game: try to come up with an expression that behaves differently on ALL
# FOUR shells.

#### ${##}
set -- $(seq 25)
echo ${##}
## stdout: 2

#### ${###}
set -- $(seq 25)
echo ${###}
## stdout: 25
## OK osh stdout-json: ""
## OK osh status: 2

#### ${####}
set -- $(seq 25)
echo ${####}
## stdout: 25
## OK osh stdout-json: ""
## OK osh status: 2

#### ${##2}
set -- $(seq 25)
echo ${##2}
## stdout: 5
## OK osh stdout-json: ""
## OK osh status: 2

#### ${###2}
set -- $(seq 25)
echo ${###2}
## stdout: 5
## BUG mksh stdout: 25
## OK osh stdout-json: ""
## OK osh status: 2

#### ${1####}
set -- '####'
echo ${1####}
## stdout: ##

#### ${1#'###'}
set -- '####'
echo ${1#'###'}
## stdout: #

