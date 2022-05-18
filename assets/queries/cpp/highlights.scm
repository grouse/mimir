[
 "const"
 "default"
 "enum"
 "extern"
 "inline"
 "static"
 "struct"
 "typedef"
 "union"
 "volatile"
 "goto"
 "register"
 "return"
 "class"
 "decltype"
 "constexpr"
 "explicit"
 "final"
 "friend"
 "mutable"
 "namespace"
 "override"
 "private"
 "protected"
 "public"
 "template"
 "typename"
 "using"
 "virtual"
 "co_await"
 "concept"
 "requires"
 "consteval"
 "constinit"
 "co_yield"
 "co_return"
 (auto)
] @keyword

[
 "sizeof"
 "new"
 "delete"
] @keyword.operator

[
 "try"
 "catch"
 "noexcept"
 "throw"
] @keyword.exception

[
  "while"
  "for"
  "do"
  "continue"
  "break"
] @keyword.control

[
 "if"
 "else"
 "case"
 "switch"
] @keyword.branch

[
 "#if"
 "#ifdef"
 "#ifndef"
 "#else"
 "#elif"
 "#endif"
 "#define"
 (preproc_directive)
] @preproc

"#include" @include

[
  "="

  "-"
  "*"
  "/"
  "+"
  "%"

  "~"
  "|"
  "&"
  "^"
  "<<"
  ">>"

  "->"

  "<"
  "<="
  ">="
  ">"
  "=="
  "!="

  "!"
  "&&"
  "||"

  "-="
  "+="
  "*="
  "/="
  "%="
  "|="
  "&="
  "^="
  ">>="
  "<<="
  "--"
  "++"
  "<=>"
  "::"
] @operator

[
 (true)
 (false)
] @constant.boolean

(conditional_expression [ "?" ":" ] @keyword.branch)

[ "(" ")" "[" "]" "{" "}"] @punctuation.bracket
[ "." ";" ":" "," ] @punctuation.delimiter


[
 (string_literal)
 (system_lib_string) 
 (char_literal)
 (raw_string_literal)
] @string

[
 (null)
 (nullptr)
] @constant.builtin

(number_literal) @constant.number

(preproc_def name: (identifier) @preproc.identifier)
(preproc_function_def name: (identifier) @preproc.identifier)

(((field_expression
     (field_identifier) @property)) @_parent
 (#not-has-parent? @_parent template_method function_declarator call_expression))

(((field_identifier) @property)
 (#has-ancestor? @property field_declaration)
 (#not-has-ancestor? @property function_declarator))

(statement_identifier) @label

[
 (type_identifier)
 (sized_type_specifier)
 (type_descriptor)
] @type

(primitive_type) @type.primitive

(sizeof_expression value: (parenthesized_expression (identifier) @type))
(concept_definition name: (identifier) @type)

(enumerator name: (identifier) @constant)
(case_statement value: (identifier) @constant)

(preproc_def
  name: (_) @unused)
(preproc_call
  directive: (preproc_directive) @_u
  argument: (_) @unused
  (#eq? @_u "#undef"))

(call_expression function: (identifier) @function.call)
(call_expression function: (field_expression field: (field_identifier) @function.call))
(function_declarator declarator: (identifier) @function.decl)

(parameter_declaration declarator: (identifier) @parameter)
(parameter_declaration declarator: (pointer_declarator) @parameter)
(preproc_params (identifier) @parameter)

[
  "__attribute__"
  "__cdecl"
  "__clrcall"
  "__stdcall"
  "__fastcall"
  "__thiscall"
  "__vectorcall"
  "_unaligned"
  "__unaligned"
  "__declspec"
] @keyword.attribute

(ERROR) @error

(function_declarator declarator: (field_identifier) @function.decl)

(case_statement value: (qualified_identifier (identifier) @constant))

(template_function name: (identifier) @function.decl.template)
(template_method name: (field_identifier) @function.decl.template)

(using_declaration . "using" . "namespace" . [(qualified_identifier) (identifier)] @namespace)

(function_declarator declarator: (qualified_identifier name: (identifier) @function))
(function_declarator declarator: (qualified_identifier name: (qualified_identifier name: (identifier) @function)))

(operator_name) @function.operator
"operator" @function.operator
"static_assert" @function.builtin

(destructor_name (identifier) @function.destructor)
(call_expression function: (qualified_identifier name: (identifier) @function))
(call_expression function: (qualified_identifier name: (qualified_identifier name: (identifier) @function)))
(call_expression function: (qualified_identifier name: (qualified_identifier name: (qualified_identifier name: (identifier) @function))))
(call_expression function: (field_expression field: (field_identifier) @function))

(this) @keyword

(comment) @comment

; Why doesn't this work? It's only supposed to match upper case identifiers
; but it's matching all of them
;((identifier) @constant (#lua-match? @constant "^[A-Z][A-Z0-9_]+$"))
