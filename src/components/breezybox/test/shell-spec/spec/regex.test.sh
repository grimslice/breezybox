## oils_failures_allowed: 0
## compare_shells: bash-4.4 zsh

#
# Only bash and zsh seem to implement [[ foo =~ '' ]]
#
# ^(a b)$ is a regex that should match 'a b' in a group.
#
# Not sure what bash is doing here... I think I have to just be empirical.
# Might need "compat" switch for parsing the regex.  It should be an opaque
# string like zsh, not sure why it isn't.
#
# I think this is just papering over bugs...
# https://www.gnu.org/software/bash/manual/bash.html#Conditional-Constructs
#
# Storing the regular expression in a shell variable is often a useful way to
# avoid problems with quoting characters that are special to the shell. It is
# sometimes difficult to specify a regular expression literally without using
# quotes, or to keep track of the quoting used by regular expressions while
# paying attention to the shell’s quote removal. Using a shell variable to
# store the pattern decreases these problems. For example, the following is
# equivalent to the above:
#
# pattern='[[:space:]]*(a)?b'
# [[ $line =~ $pattern ]]
# 
# If you want to match a character that’s special to the regular expression
# grammar, it has to be quoted to remove its special meaning. This means that in
# the pattern ‘xxx.txt’, the ‘.’ matches any character in the string (its usual
# regular expression meaning), but in the pattern ‘"xxx.txt"’ it can only match a
# literal ‘.’. Shell programmers should take special care with backslashes, since
# backslashes are used both by the shell and regular expressions to remove the
# special meaning from the following character. The following two sets of
# commands are not equivalent: 
#
# From bash code: ( | ) are treated special.  Normally they must be quoted, but
# they can be UNQUOTED in BASH_REGEX state.  In fact they can't be quoted!

#### Regex with == and not =~ is parse error, different lexer mode required
# They both give a syntax error.  This is lame.
[[ '^(a b)$' == ^(a\ b)$ ]] && echo true
## status: 2
## OK zsh status: 1

#### Malformed regex
# Are they trying to PARSE the regex?  Do they feed the buffer directly to
# regcomp()?
[[ 'a b' =~ ^)a\ b($ ]] && echo true
## stdout-json: ""
## status: 2
## OK zsh status: 1

