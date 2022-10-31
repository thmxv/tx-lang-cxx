# Values

## Builting types

### Nil

The `nil` value is a special value indicating the absence of a value. It is 
the only instance of the `Nil` type.

### Booleans

A boolean represant a binary value, `true` and `false` are the only possible 
instances of the `Bool` type.

### Integer numbers

Integer numbers and floating point numbers have distinc types in Tx.

Integer numbers are instances of the `Int` type. And integer numeral are 
composed of an optional `-` prefix followed by digits only.

    0
    1234
    -5678

The size and maximum/minimum values of and integer is implementation 
dependant, but should at least allow all the values of conventional 32 bits 
integers. In practice, integers are either 32, 48 or 64 bits wide, depending
on compilation options.

    Int::MAX    # The smallest integer representable
    Int::MIN    # The largest integer representable
    Int::BITS   # The number of usable bits


### Floating point numbers

Floating point numbers are instances of the `Float` type and are either single
or double precision depending on the implementation. Number literals are 
allowed in the form of a series of digits with a decimal point or in scientific
notation of in hexadecimal notation.

    3.14159
    1.0
    -12.34
    0.0314159e02
    0.0314159e+02
    314.159e-02
    0xcaffe2

Limits

    Float::MIN              # The smallest finite possible value
    Float::MIN_POSITIVE     # The smallest positive possible value
    Float::MAX              # The largest finite possible value
    Float::NAN              # Not a Number
    Float::INF              # Infinity
    Float::NEG_INF          # Negative infinity
    Float::EPSILON          # The difference between 1.0 and the next value


### Strings

Stings are instance of the `Str` type. A string is an array of byte, usually 
representing a string of character encoded in UTF-8. String literals are 
surrounded by double quotes:

    "Hello, world!"

#### Escaping

Some characters need to be escaped with `\`

    "\0"            # The NUL byte: 0.
    "\""            # A double quote character.
    "\\"            # A backslash.
    "\$"            # A dollar sign.
    "\a"            # Alarm beep.
    "\b"            # Backspace.
    "\e"            # ESC character.
    "\f"            # Formfeed.
    "\n"            # Newline.
    "\r"            # Carriage return.
    "\t"            # Tab.
    "\v"            # Vertical tab.
    "\x48"          # Unencoded byte     (2 hex digits)
    "\u0041"        # Unicode code point (4 hex digits)
    "\U0001F64A"    # Unicode code point (8 hex digits)

#### Interpolation

Not implemented yetl

#### Raw strings

Raw string literals are surrounded by 3 double quotes `"""` and do not 
process escaping or interpolation but leading/trailing whitespaces are ignored 
when the 3 double quotes are on a line by themselves.

    # Those 2 strings are identical and do not contain any newlines.

    """Hello, world!"""

    """
    Hello, world!
    """

### Characters

Not implemented yet.

Charaters are instances of the `Char` type and allow to store any unicode 
code point. Charaters literals are not allowed you need to get a char from 
a string.

### Arrays

Arrays are ordered collections of values accessible by an integer index. Arrays 
are zero indexed (the index of the first element is 0).

    [1, "hello", true]
    [1, "hello", true,]     # trailing coma is allowed

    var list : Array<Int> = [];
    list.push(1);
    list[0] == 5;

### Maps

TODO 

    ["key_string" : 1, nil : "value_string", 0 : 1, ]

    var map : Map<Any, Any> = [];
    map["hello"] = 5;


## User types

TODO 

    struct S {
        name: Str,
        age: Int,   # trailing comma allowed
    };


