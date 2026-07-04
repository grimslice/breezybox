## oils_failures_allowed: 0
## compare_shells: dash bash mksh zsh ash

#### command local

f() {
  command local s=local
  echo s=$s
}

f

## STDOUT:
s=local
## END

## BUG dash/ash STDOUT:
s=
## END

## OK mksh/zsh STDOUT:
s=
## END

