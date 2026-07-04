## compare_shells: bash dash mksh zsh ash
## oils_failures_allowed: 1

#### echo keyword
echo done
## stdout: done

#### if/else
if false; then
  echo THEN
else
  echo ELSE
fi
## stdout: ELSE

#### First word like foo$x() and foo$[1+2] (regression)

# Problem: $x() func call broke this error message
foo$identity('z')

foo$[1+2]

echo DONE

## status: 2
## OK mksh/zsh status: 1
## STDOUT:
## END

#### Function names
foo$x() {
  echo hi
}

foo $x() {
  echo hi
}

## status: 2
## OK mksh status: 1
# Note: zsh should return 1 or 2
## BUG zsh status: 0
## STDOUT:
## END


#### subshell while running a script (regression)
# Ensures that spawning a subshell doesn't cause a seek on the file input stream
# representing the current script (issue #1233).
cat >tmp.sh <<'EOF'
echo start
(:)
echo end
EOF
$SH tmp.sh
## STDOUT:
start
end
## END

#### autoconf word split (#1449)

mysed() {
  for line in "$@"; do
    echo "[$line]"
  done
}

sedinputs="f1 f2"
sedscript='my sed command'

# Parsed and evaluated correctly: with word_part.EscapedLiteral \"

x=$(eval "mysed -n \"\$sedscript\" $sedinputs")
echo '--- $()'
echo "$x"

# With backticks, the \" gets lost somehow

x=`eval "mysed -n \"\$sedscript\" $sedinputs"`
echo '--- backticks'
echo "$x"


# Test it in a case statement

case `eval "mysed -n \"\$sedscript\" $sedinputs"` in 
  (*'[my sed command]'*)
    echo 'NOT SPLIT'
    ;;
esac

## STDOUT:
--- $()
[-n]
[my sed command]
[f1]
[f2]
--- backticks
[-n]
[my sed command]
[f1]
[f2]
NOT SPLIT
## END

#### autoconf arithmetic - relaxed eval_unsafe_arith (#1450)

as_fn_arith ()
{
    as_val=$(( $* ))
}
as_fn_arith 1 + 1
echo $as_val

## STDOUT:
2
## END

#### printf integer size bug

# from Koiche on Zulip

printf '%x\n' 2147483648
printf '%u\n' 2147483648
## STDOUT:
80000000
2147483648
## END

#### autotools as_fn_arith bug in configure

# Causes 'grep -e' check to infinite loop.
# Reduced from a configure script.

as_fn_arith() {
  as_val=$(( $* ))
}

as_fn_arith 0 + 1
echo as_val=$as_val
## STDOUT:
as_val=1
## END

#### Crash in {1..10} - issue #2296

{1..10}

## status: 127
## STDOUT:
## END

