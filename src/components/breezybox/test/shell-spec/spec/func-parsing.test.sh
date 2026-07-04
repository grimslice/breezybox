## compare_shells: dash bash mksh

#### Incomplete Function
## code: foo()
## status: 2
## BUG mksh status: 0

#### Incomplete Function 2
## code: foo() {
## status: 2
## OK mksh status: 1

#### Bad function
## code: foo(ls)
## status: 2
## OK mksh status: 1

#### Unbraced function body. 
# dash allows this, but bash does not.  The POSIX grammar might not allow
# this?  Because a function body needs a compound command.
# function_body    : compound_command
#                  | compound_command redirect_list  /* Apply rule 9 */
## code: one_line() ls; one_line;
## status: 0
## OK bash/osh status: 2

#### Function with spaces, to see if ( and ) are separate tokens.
# NOTE: Newline after ( is not OK.
fun ( ) { echo in-func; }; fun
## stdout: in-func

#### subshell function
# bash allows this.
i=0
j=0
inc() { i=$((i+5)); }
inc_subshell() ( j=$((j+5)); )
inc
inc_subshell
echo $i $j
## stdout: 5 0

#### Hard case, function with } token in it
rbrace() { echo }; }; rbrace
## stdout: }

#### Break after ) is OK.
# newline is always a token in "normal" state.
echo hi; fun ( )
{ echo in-func; }
fun
## STDOUT:
hi
in-func
## END

#### Nested definition
# A function definition is a command, so it can be nested
fun() {
  nested_func() { echo nested; }
  nested_func
}
fun
## stdout: nested

