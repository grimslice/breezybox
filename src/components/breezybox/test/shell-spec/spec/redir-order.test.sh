## oils_failures_allowed: 0

#### echo `cat OSCFLAGS` "world" > OSCFLAGS (from Alpine imap)

echo hello > OSCFLAGS
echo `cat OSCFLAGS` "world" > OSCFLAGS
#echo $(cat OSCFLAGS) "world" > OSCFLAGS
cat OSCFLAGS

## STDOUT:
hello world
## END

#### subshell + redirect order

echo hello > OSCFLAGS
(echo `cat OSCFLAGS` "world") > OSCFLAGS
cat OSCFLAGS

## STDOUT:
world
## END

#### for word + redirect order

echo hello > OSCFLAGS
for x in `cat OSCFLAGS` world; do
  echo $x
done > OSCFLAGS
cat OSCFLAGS

## STDOUT:
world
## END

#### case word + redirect order

echo hello > OSCFLAGS
case `cat OSCFLAGS` in
  hello)
    echo hello
    ;;
  *)
    echo other
    ;;
esac > OSCFLAGS
cat OSCFLAGS

## STDOUT:
other
## END

