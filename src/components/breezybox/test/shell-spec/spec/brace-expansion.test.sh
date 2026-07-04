## compare_shells: bash mksh zsh
## oils_failures_allowed: 0

#### no expansion
echo {foo}
## stdout: {foo}

#### { in expansion
# bash and mksh treat this differently.  bash treats the
# first { is a prefix.  I think it's harder to read, and \{{a,b} should be
# required.
echo {{a,b}
## stdout: {{a,b}
## BUG bash/zsh stdout: {a {b

#### expansion on RHS of assignment
# I think bash's behavior is more consistent.  No splitting either.
v={X,Y}
echo $v
## stdout: {X,Y}
## BUG mksh stdout: X Y

#### no expansion with RHS assignment
{v,x}=X
## status: 127
## stdout-json: ""
## OK zsh status: 1

#### Tilde expansion
HOME=/home/foo
echo ~
HOME=/home/bar
echo ~
## STDOUT:
/home/foo
/home/bar
## END

#### Tilde expansion come before var expansion
HOME=/home/bob
foo=~
echo $foo
foo='~'
echo $foo
# In the second instance, we expand into a literal ~, and since var expansion
# comes after tilde expansion, it is NOT tried again.
## STDOUT:
/home/bob
~
## END

#### Invalid brace expansions don't expand
echo {1.3}
echo {1...3}
echo {1__3}
## STDOUT:
{1.3}
{1...3}
{1__3}
## END

#### Invalid brace expansions mixing characters and numbers
# zsh does something crazy like : ; < = > that I'm not writing
case $SH in *zsh) echo BUG; exit ;; esac
echo {1..a}
echo {z..3}
## STDOUT:
{1..a}
{z..3}
## END
## BUG zsh STDOUT:
BUG
## END

