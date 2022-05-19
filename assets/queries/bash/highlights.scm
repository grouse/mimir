
[
  (string)
  (raw_string)
  (heredoc_body)
  (heredoc_start)
] @string

(variable_name) @property
(command_name) @function

[
  "case"
  "do"
  "done"
  "elif"
  "else"
  "esac"
  "export"
  "fi"
  "for"
  "function"
  "if"
  "in"
  "unset"
  "while"
  "then"
] @keyword

(comment) @comment

(function_definition name: (word) @function)

(file_descriptor) @constant.number

[
  (command_substitution)
  (process_substitution)
  (expansion)
] @embedded

[
  "$"
  "&&"
  ">"
  ">>"
  "<"
  "|"
  (expansion_flags)
] @operator

(
  (command (_) @constant)
  (#match? @constant "^-")
)
