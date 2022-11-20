# Syntax

## Comments

Line comments start with `#` and end at the end of the line.

    # This is a comment

## Keywords

This is a list of all the keywords in Tx.

    and as async await break continue else false for fn if import impl in
    inout is let loop nil match out or return self struct super trait
    true var while Any Array Bool Char Float Int Map Nil Self Str Tuple

## Identifiers

Identifiers must start with a letter or underscore and may contain letters,
digits and underscores. Identifiers are case sensitives.

    simple
    snake_case
    camelCase
    CamelCase
    ALL_CAPS
    _under_score
    abc123

## Newlines

Newlines are considered as white space and have no significance

## Statements

A Tx program consist of a series of statements. Tx have different kind of
statements, like:

- Declaration statements
- Control flow statements
- Expression statements, which are expressions followed by a semicolon `;`.

    ```
    # function declaration
    fn main() -> Nil {
        # variable declaration
        let x = 5;

        # expression statements
        x;
        x + 1;
        15;
    }
    ```

## Expressions

Expressions produces a value, and are parsed using operator precedence. 

| PREC  | OPERATORS       | DESCRIPTION                                     |
|:------|:----------------|:------------------------------------------------|
|1      | `. () [] {}`    | Function call, Grouping, Subscript, Block scope |
|2      | `! -`           | Not, Negate                                     |
|3      | `* /`           | Multiply, Divide                                |
|4      | `+ -`           | Add, Substract                                  |
|5      | `< > <= >=`     | Less-than, Greater-than                         |
|6      | `== !=`         | Equal, Not equal                                |
|7      | `and`           | Logical AND                                     |
|8      | `or`            | Logical OR                                      |
|9      | `=`             | Assignment                                      |

## Blocks

Curly braces are used to define blocks. Blocks are expressions and produce a
value. The last expression in the block is used as the block result
expression. Unless the last expression is followed by a `;` (which discard 
the expression result by making it a statement) in which case the 
result value is `nil`.
    
    let x = 5;

    let y = {
        let x_squared = x * x;
        let x_cube = x_squared * x;

        # This expression will be assigned to 'y'
        x_cube + x_squared + x
    };

    let z = {
        # The semicolon suppresses this expression and `nil` is assigned to `z`
        2 * x;
    };

In places where a statement is expected, a block can be used without being 
followed  by a `;`

    let x = 0;

    x+1;                # here a ';' is needed to make expr into statement

    { let y = 666; }    # no ';' needed after '}'

    {
        let a = 5;
    }                   # no ';' needed here neither
    
Blocks are mandatory for the bodies of functions and control flow statements. 
The result value of the block is discarted when used as the body for a 
`while` or `for` loop statement.

    if (true) { print("hello"); }

    if (true) print("hello");      # Invalid syntax 

