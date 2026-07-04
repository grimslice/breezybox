## compare_shells: bash dash mksh zsh ash
## oils_failures_allowed: 3

# This file relates to:
#
# - doc/known-differences.md
# - "divergence" tag on github:
#   https://github.com/oils-for-unix/oils/issues?q=is%3Aissue%20state%3Aopen%20label%3Adivergence

#### !( as negation and subshell versus extended glob - #2463

have_icu_uc=false
have_icu_i18n=false

if !($have_icu_uc && $have_icu_i18n); then
  echo one
fi
echo two

## STDOUT:
one
two
## END

## BUG mksh STDOUT:
two
## END

