## tags: interactive
## compare_shells: bash
## oils_failures_allowed: 2

#### fc ignores too many args
fc -l 0 1 2 || echo too many args!
## status: 0

#### fc -l when no history is present
fc -l
## status: 0
