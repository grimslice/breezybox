## compare_shells: dash bash mksh

# word-eval.test.sh: Test the word evaluation pipeline in order.
#
# Part evaluation, splitting, joining, elision, globbing.

# TODO: Rename word-eval-smoke.test.sh?
# Word sequence evaluation.
# This is more like a vertical slice.  For exhaustive tests, see:
# 
# word-split.test.sh (perhaps rename word-reframe?)
# glob.test.sh

#### Evaluation of constant parts
argv.py bare 'sq'
## stdout: ['bare', 'sq']

#### Word splitting
s1='1 2'
s2='3 4'
s3='5 6'
argv.py $s1$s2 "$s3"
## stdout: ['1', '23', '4', '5 6']

#### Word elision
s1=''
argv.py $s1 - "$s1"
## stdout: ['-', '']
