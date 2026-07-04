## compare_shells: dash bash mksh
## legacy_tmp_dir: yes

# Some nonsensical combinations which can all be detected at PARSE TIME.
# All shells allow these, but right now OSH disallowed.
# TODO: Run the parser on your whole corpus, and then if there are no errors,
# you should make OSH the OK behavior, and others are OK.

#### Prefix env on control flow
for x in a b c; do
  echo $x
  E=env break
done
## status: 0
## stdout: a
## OK osh status: 2
## OK osh stdout-json: ""

