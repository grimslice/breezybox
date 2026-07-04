## compare_shells: dash bash mksh zsh ash
## oils_failures_allowed: 2
## oils_cpp_failures_allowed: 1

#### printf - \0 escape shows NUL byte
show_hex() { od -A n -t c -t x1; }

printf '\0\n' | show_hex
## STDOUT:
  \0  \n
  00  0a
## END

#### Compare \x00 byte versus \x01 byte - read builtin

# Hm same odd behavior

show_string() {
  read s
  echo len=${#s}
  echo -n "$s" | od -A n -t x1
}

printf '.\001.' | show_string

printf '.\000.' | show_string

printf '\000' | show_string

## STDOUT:
len=3
 2e 01 2e
len=2
 2e 2e
len=0
## END

## BUG zsh STDOUT:
len=3
 2e 01 2e
len=3
 2e 00 2e
len=1
 00
## END

