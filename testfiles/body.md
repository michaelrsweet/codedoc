Body Text Heading
=================

This is a test file showing markdown being used as body text.  Here is a list:

- One fish
- Two fish
- Red fish
- Blue fish

Sample Code Sub-Heading
-----------------------

Here is some sample C code using the built-in syntax highlighting:

```c
/*
 * 'foo_void_function()' - Do foo with bar.
 *
 * Use the @link foo_float_function@ or @link foo_int_function@ functions
 * instead.  Pass @code NULL@ for "three" then there is no string to print.
 *
 * - List item 1
 * - List item 2 is longer and spans
 *   two lines.
 * - List item 3
 *
 * This is a code example:
 *
 * ```c
 * | foo_void_function(1, 2.0f, "3");
 * |
 * | if (bar)
 * |   foo_void_function(2, 4.0f, "6");
 * ```
 *
 * This is a paragraph following the code example with `code`, *emphasized*,
 * and **strongly emphasized** text.
 *
 * > Note: This is a block quote.  Block quotes can span multiple lines and
 * > contain *emphasized*, **strongly emphasized**, and `code` text, along
 * > with [links](https://www.example.com/).
 *
 * <https://www.msweet.org/codedoc> is the home page.
 *
 * File bugs [here](https://github.com/michaelrsweet/codedoc/issues).
 *
 * @deprecated@
 */

void
foo_void_function(int        one,       // I - Integer
                  float      *two,      // O - Real number
                  const char *three)    // I - String
{
  if (one)
  {
    puts("Hello, World!");
  }
  else
    puts(three);

  *two = 2.0f;
}
```

Here is an HTML file that is highlighted as code:

```html
<!DOCTYPE html>
<html>
  <!-- This is a comment. -->
  <head>
    <title>This is a Title</title>
  </head>
  <body>
    <h1>This is a Heading</h1>
    <p>All work and no play makes Johnny a dull boy.</p>
    <p><a href="https://www.msweet.org/codedoc">Link to codedoc home page.</a>
    <a
     href="https://www.msweet.org/codedoc">Link that crosses a line.</a></p>
  </body>
</html>
```

Here is an XML file that is highlighted as code:

```xml
<?xml version="1.0" encoding="utf-8"?>
<data>
    <node attr="value">val1</node>
    <node>val2</node>
    <node>val3</node>
    <group>
        <node>val4</node>
        <node>val5</node>
        <node>val6</node>
    </group>
    <node>val7</node>
    <node
     attr="value">val8</node>
</data>
```
