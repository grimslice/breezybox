## oils_failures_allowed: 3

# Hay: Hay Ain't YAML

#### Parsing Nested Attributes nodes (bug fix)

shopt --set parse_brace parse_equals

hay define Package/License

Package glibc {
  version = '1.0'

  License {
    path = 'LICENSE.txt'
  }

  other = 'foo'
}

json write (_hay()) | jq '.children[0].children[0].attrs' > actual.txt

diff -u - actual.txt <<EOF
{
  "path": "LICENSE.txt"
}
EOF

invalid = 'syntax'  # parse error

## status: 2
## STDOUT:
## END

