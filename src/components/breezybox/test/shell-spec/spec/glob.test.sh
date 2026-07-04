## oils_failures_allowed: 3
## compare_shells: bash dash mksh ash
## legacy_tmp_dir: yes

#### glob double quote escape
echo "*.sh"
## stdout: *.sh

#### glob single quote escape
echo "*.sh"
## stdout: *.sh

#### glob backslash escape
echo \*.sh
## stdout: *.sh

#### 1 char glob
cd $REPO_ROOT
echo [b]in
## stdout: bin

#### 0 char glob -- does NOT work
echo []bin
## STDOUT:
[]bin
## END

#### looks like glob at the start, but isn't
echo [bin
## stdout: [bin

#### looks like glob plus negation at the start, but isn't
echo [!bin
## stdout: [!bin

#### quoted var expansion with glob meta characters
touch _tmp/a.A _tmp/aa.A _tmp/b.B
f="_tmp/*.A"
echo "[ $f ]"
## stdout: [ _tmp/*.A ]

#### glob after "$@" expansion
fun() {
  echo "$@"
}
fun '_tmp/*.B'
## stdout: _tmp/*.B

#### no glob after ~ expansion
HOME=*
echo ~/*.py
## stdout: */*.py

#### set -o noglob (bug #698)
var='\z'
set -f
echo $var
## STDOUT:
\z
## END

#### Glob of unescaped [[] and []]
touch $TMP/[ $TMP/]
cd $TMP
echo [\[z] [\]z]  # the right way to do it
echo [[z] []z]    # also accepted
## STDOUT:
[ ]
[ ]
## END

#### \ in unquoted substitutions is preserved
v='\*\*.txt'
echo $v
echo "$v"

## STDOUT:
\*\*.txt
\*\*.txt
## END


#### \ in unquoted substitutions is preserved with set -o noglob
set -f
v='*\*.txt'
echo $v

## STDOUT:
*\*.txt
## END


#### \ in unquoted substitutions is preserved without glob matching
mkdir x
touch \
  'x/test.ifs.\.txt' \
  'x/test.ifs.*.txt'
v='*\*.txt'
argv.py x/unmatching.$v

## STDOUT:
['x/unmatching.*\\*.txt']
## END

