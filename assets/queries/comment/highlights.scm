[
 "("
 ")"
] @punctuation.bracket

":" @punctuation.delimiter

(tag (name) @text.note (user)? @constant)
((tag ((name) @text.warning)) (#any-of? @text.warning "TODO" "HACK" "WARNING"))

;("text" @text.warning (#any-of? @text.warning "TODO" "HACK" "WARNING"))
;((tag ((name) @text.danger)) (#any-of? @text.danger "FIXME" "XXX" "BUG"))
;("text" @text.danger (#any-of? @text.danger "FIXME" "XXX" "BUG"))
