;;; Can handle binary expressions with funcitons
; local x = vim.x() or vim.y()
(program
  (variable_declaration
    (local)
    (variable_declarator
      (identifier))
    (binary_operation
      (function_call
        (identifier)
        (identifier)
        (function_call_paren)
        (function_call_paren))
      (function_call
        (identifier)
        (identifier)
        (function_call_paren)
        (function_call_paren)))))
