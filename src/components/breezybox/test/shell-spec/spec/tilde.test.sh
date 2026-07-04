## compare_shells: bash dash mksh zsh

#### ~ expansion in assignment
HOME=/home/bob
a=~/src
echo $a
## stdout: /home/bob/src

#### No tilde expansion in word that looks like assignment but isn't
# bash and mksh mistakenly expand here!
# bash fixes this in POSIX mode (gah).
# http://lists.gnu.org/archive/html/bug-bash/2016-06/msg00001.html
HOME=/home/bob
echo x=~
## stdout: x=~
## BUG bash/mksh stdout: x=/home/bob

#### tilde expansion of word after redirect
HOME=$TMP
echo hi > ~/tilde1.txt
cat $HOME/tilde1.txt | wc -c
## stdout: 3
## status: 0

#### other user
echo ~nonexistent
## stdout: ~nonexistent
# zsh doesn't like nonexistent
## OK zsh stdout-json: ""
## OK zsh status: 1

#### ${undef:-~}
HOME=/home/bar
echo ${undef:-~}
echo ${HOME:+~/z}
echo "${undef:-~}"
echo ${undef:-"~"}
## STDOUT:
/home/bar
/home/bar/z
~
~
## END
