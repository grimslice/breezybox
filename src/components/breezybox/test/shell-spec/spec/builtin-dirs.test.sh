## compare_shells: bash zsh

# dash and mksh don't implement 'dirs'

#### pushd does not take more than one argument
pushd . . >/dev/null || echo too many args!
## OK zsh STDOUT:
## END
## STDOUT:
too many args!
## END

#### dirs does not take arguments
dirs a || echo failed
dirs -l a || echo failed
## STDOUT:
failed
failed
## END
## BUG zsh STDOUT:
## END
