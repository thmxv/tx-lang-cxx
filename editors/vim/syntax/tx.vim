" Vim syntax file
" Language: Tx
" Latest Revision: 2022-11-02

if exists("b:current_syntax")
  finish
endif

let s:cpo_save = &cpo
set cpo&vim

syn keyword	cTodo		contained TODO FIXME XXX

let s:tx_syntax_keywords = {
    \   'txConditional' :["if"
    \ ,                     "else"
    \ ,                     "match"
    \ ,                    ]
    \ , 'txRepeat' :["while"
    \ ,                "for"
    \ ,                "loop"
    \ ,               ]
    \ , 'txExecution' :["return"
    \ ,                   "break"
    \ ,                   "continue"
    \ ,                   "yield"
    \ ,                  ]
    \ , 'txBoolean' :["true"
    \ ,                 "false"
    \ ,                ]
    \ , 'txKeyword' :["fn"
    \ ,                 "import"
    \ ,                 "impl"
    \ ,                 "trait"
    \ ,                 "async"
    \ ,                 "await"
    \ ,                ]
    \ , 'txWordOperator' :["not"
    \ ,                      "and"
    \ ,                      "or"
    \ ,                      "as"
    \ ,                      "is"
    \ ,                     ]
    \ , 'txVarDecl' :["var"
    \ ,                 "let"
    \ ,                 "in"
    \ ,                 "inout"
    \ ,                 "out"
    \ ,                ]
    \ , 'txType' :["Str"
    \ ,              "Nil"
    \ ,              "Bool"
    \ ,              "Int"
    \ ,              "Float"
    \ ,              "Char"
    \ ,              "Fn"
    \ ,              "Array"
    \ ,              "Map"
    \ ,              "Self"
    \ ,              "Tuple"
    \ ,              "Any"
    \ ,             ]
    \ , 'txConstant' :["self"
    \ ,              "nil"
    \ ,                 ]
    \ , 'txStructure' :["struct"
    \ ,                  ]
    \ , }

function! s:syntax_keyword(dict)
  for key in keys(a:dict)
    execute 'syntax keyword' key join(a:dict[key], ' ')
  endfor
endfunction

call s:syntax_keyword(s:tx_syntax_keywords)

syntax match txDecNumber display "\v<\d%(_?\d)*"
syntax match txDecNumber display "\v<\d%(_?\d)*\.\d%(_?\d)*"
syntax match txDecNumber display "\v<\d%(_?\d)*[eE][+-]=\d%(_?\d)*"
syntax match txDecNumber display "\v<\d%(_?\d)*\.\d%(_?\d)*[eE][+-]=\d%(_?\d)*"
syntax match txHexNumber display "\v<0x\x%(_?\x)*"
" syntax match txOctNumber display "\v<0o\o%(_?\o)*"
" syntax match txBinNumber display "\v<0b[01]%(_?[01])*"

" syntax match txFatArrowOperator display "\V=>"
" syntax match txRangeOperator display "\V.."
syntax match txOperator display "\V\[-+/*=^&?|!><%~:;,]"

syntax match txFunction /\w\+\s*(/me=e-1,he=e-1

" syntax match txEnumDecl /enum\s\+\w\+/lc=4
syntax match txStructDecl /struct\s\+\w\+/lc=6

syntax region txBlock start="{" end="}" transparent fold

syntax region txCommentLine start="#" end="$"


syntax region txString matchgroup=txStringDelimiter 
    \ start=+"+ skip=+\\\\\|\\"+ end=+"+ contains=txEscape
syntax region txString matchgroup=txStringDelimiter 
    \ start=+"""+ skip=+\\\\\|\\"+ end=+"""+ contains=txEscape
" syntax region txChar matchgroup=txCharDelimiter 
"     \ start=+'+ skip=+\\\\\|\\'+ end=+'+ oneline contains=txEscape
syntax match txEscape display contained /\\./

highlight default link txDecNumber txNumber
highlight default link txHexNumber txNumber
" highlight default link txOctNumber txNumber
" highlight default link txBinNumber txNumber

highlight default link txWordOperator txOperator
" highlight default link txFatArrowOperator txOperator
" highlight default link txRangeOperator txOperator

highlight default link txStructDecl txType
" highlight default link txEnumDecl txType

highlight default link txKeyword Keyword
highlight default link txType Type
highlight default link txCommentLine Comment
highlight default link txString String
highlight default link txStringDelimiter String
" highlight default link txChar String
" highlight default link txCharDelimiter String
highlight default link txEscape Special
highlight default link txBoolean Boolean
highlight default link txConstant Constant
highlight default link txNumber Number
highlight default link txOperator Operator
highlight default link txStructure Structure
highlight default link txExecution Special
highlight default link txConditional Conditional
highlight default link txRepeat Repeat
highlight default link txVarDecl Define
highlight default link txFunction Function
" highlight default link txLabel Label

delfunction s:syntax_keyword

let b:current_syntax = "tx"

let &cpo = s:cpo_save
unlet! s:cpo_save
