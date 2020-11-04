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
 * ```
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
