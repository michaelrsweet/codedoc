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
foo_void_function(int        one,	/* I - Integer */
                  float      *two,	/* O - Real number */
                  const char *three)	/* I - String */
{
  if (one)
  {
    puts("Hello, World!");
  }
  else
    puts(three);

  *two = 2.0f;
}


//
// 'foo_float_function()' - Do foo with bar.
//
// @since 1.2@
//

float					/* O - Real number */
foo_float_function(int        one,	/* I - Integer */
                   const char *two)	/* I - String */
{
  if (one)
  {
    puts("Hello, World!");
  }
  else
    puts(two);

  return (2.0f);
}


//
// 'foo_default_string()' - Do something with a defaulted string arg.
//
// This function does something with the defaulted string argument "two".
// Example usage:
//
// ```
// void foo(void)
// {
//   foo_default_string(1);
//   foo_default_string(2, "3");
//   ... more calls ...
// }
// ```
//

int					/* O - Integer value */
foo_default_string(int one,		/* I - Integer */
                   const char *two = "2")
					/* I - String */
{
  if (one)
  {
    puts("Hello, World!");
  }
  else
    puts(two);

  return (2);
}


/*
 * 'foo_default_int()' - Do something with a defaulted int arg.
 *
 * This function does something with the defaulted int argument "two".
 * Example usage:
 *
 * ```
 * void bar(void)
 * {
 *   foo_default_int(1);
 *   foo_default_int(2, 3);
 *   ... more calls ...
 * }
 * ```
 */

int					/* O - Integer value */
foo_default_int(int one,		/* I - Integer */
                int two = 2)		/* I - Integer */
{
  if (one)
  {
    puts("Hello, World!");
  }
  else
    puts(two);

  return (2);
}


/*
 * 'foo_void_func()' - Function taking no arguments.
 */

void
foo_void_func(void)
{
  puts("foo_void_func()");
}


/*
 * 'foo_private_func()' - Private function.
 *
 * @private@
 */

void
foo_private_func(void)
{
  puts("foo_private_func()");
}
