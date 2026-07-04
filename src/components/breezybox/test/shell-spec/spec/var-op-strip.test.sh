## compare_shells: dash bash mksh zsh ash

#### Remove const suffix
v=abcd
echo ${v%d} ${v%%cd}
## stdout: abc ab

#### Remove const prefix
v=abcd
echo ${v#a} ${v##ab}
## stdout: bcd cd

#### Remove const suffix from undefined
echo ${undef%suffix}
## stdout:

#### Remove shortest glob suffix
v=aabbccdd
echo ${v%c*}
## stdout: aabbc

#### Remove longest glob suffix
v=aabbccdd
echo ${v%%c*}
## stdout: aabb

#### Remove shortest glob prefix
v=aabbccdd
echo ${v#*b}
## stdout: bccdd

#### Remove longest glob prefix
v=aabbccdd
echo ${v##*b}
## stdout: ccdd

#### Strip char class
v=abc
echo ${v%[[:alpha:]]}
## stdout: ab
## OK mksh stdout: abc

#### Bug fix: Test that you can remove everything with glob
s='--x--'
argv.py "${s%%-*}" "${s%-*}" "${s#*-}" "${s##*-}"
## STDOUT:
['', '--x-', '-x--', '']
## END

#### Test that you can remove everything with const
s='abcd'
argv.py "${s%%abcd}" "${s%abcd}" "${s#abcd}" "${s##abcd}"
# failure case:
argv.py "${s%%abcde}" "${s%abcde}" "${s#abcde}" "${s##abcde}"
## STDOUT:
['', '', '', '']
['abcd', 'abcd', 'abcd', 'abcd']
## END

#### strip * (bug regression)
x=abc
argv.py "${x#*}"
argv.py "${x##*}"
argv.py "${x%*}"
argv.py "${x%%*}"
## STDOUT:
['abc']
['']
['abc']
['']
## END
## BUG zsh STDOUT:
['abc']
['']
['ab']
['']
## END

#### strip ?
x=abc
argv.py "${x#?}"
argv.py "${x##?}"
argv.py "${x%?}"
argv.py "${x%%?}"
## STDOUT:
['bc']
['bc']
['ab']
['ab']
## END

#### strip all
x=abc
argv.py "${x#abc}"
argv.py "${x##abc}"
argv.py "${x%abc}"
argv.py "${x%%abc}"
## STDOUT:
['']
['']
['']
['']
## END

#### strip none
x=abc
argv.py "${x#}"
argv.py "${x##}"
argv.py "${x%}"
argv.py "${x%%}"
## STDOUT:
['abc']
['abc']
['abc']
['abc']
## END

#### Strip Right Brace (#702)
var='$foo'
echo 1 "${var#$foo}"
echo 2 "${var#\$foo}"

var='}'
echo 10 "${var#}}"
echo 11 "${var#\}}"
echo 12 "${var#'}'}"
echo 13 "${var#"}"}"
## STDOUT:
1 $foo
2 
10 }}
11 
12 
13 
## END
## BUG zsh STDOUT:
1 $foo
2 
10 }}
11 
12 }'}
13 
## END

#### \(\) in pattern (regression)
x='foo()' 
echo 1 ${x%*\(\)}
echo 2 ${x%%*\(\)}
echo 3 ${x#*\(\)}
echo 4 ${x##*\(\)}
## STDOUT:
1 foo
2
3
4
## END

