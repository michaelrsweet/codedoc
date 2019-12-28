namespace MYNAMESPACE {

typedef int simple_t;			/* Simple integer type */

typedef struct data_s			/* Data structure */
{
  float	a;				/* Real number */
  int	b;				/* Integer */

  data_s(float f, int b);
  ~data_s();

} data_t;

// 'data_s::data_s()' - Create a data_s structure.
data_s::data_s(float f, // I - Value of foo
               int   b) // I - Value of bar
{
  foo = f;
  bar = b;
}

// 'data_s::~data_s()' - Destroy a data_s structure.
data_s::~data_s()
{
}


typedef enum list_e			/* List enumeration type */
{
  LIST_ONE,				/* One fish */
  LIST_TWO,				/* Two fish */
  LIST_RED,				/* Red fish */
  LIST_BLUE,				/* Blue fish */
} list_t;

class data2_c : public database_c	// Data class derived from base
{
  float	a;				/* Real number */
  int	b;				/* Integer */

  public:

  data2_c(float f, int b);
  ~data2_c();
}

// 'data2_c::data2_c()' - Create a data2_c class.
data2_c::data2_c(float f, // I - Value of foo
                 int   b) // I - Value of bar
{
  foo = f;
  bar = b;
}

// 'data2_c::~data2_c()' - Destroy a data2_c class.
data2_c::~data2_c()
{
}

/*
 * 'void_function()' - Do foo with bar.
 *
 * Use the @link MYNAMESPACE::float_function@ or
 * @link MYNAMESPACE::int_function@ functions instead.  Pass @code NULL@ for
 * "three" then there is no string to print.
 *
 * - List item 1
 * - List item 2 is longer and spans
 *   two lines.
 * - List item 3
 *
 * This is a code example:
 *
 * ```
 * | MYNAMESPACE::void_function(1, 2.0f, "3");
 * |
 * | if (bar)
 * |   MYNAMESPACE::void_function(2, 4.0f, "6");
 * ```
 *
 * This is a paragraph following the code example with `code`, *emphasized*,
 * and **strongly emphasized** text.
 *
 * <https://www.msweet.org/codedoc> is the home page.
 *
 * File bugs [here](https://github.com/michaelrsweet/codedoc/issues).
 *
 * @deprecated@
 */

void
void_function(int        one,		/* I - Integer */
	      float      *two,		/* O - Real number */
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


/*
 * 'float_function()' - Do foo with bar.
 *
 * @since 1.2@
 */

float					/* O - Real number */
float_function(int        one,		/* I - Integer */
               const char *two)		/* I - String */
{
  if (one)
  {
    puts("Hello, World!");
  }
  else
    puts(two);

  return (2.0f);
}


/*
 * 'default_string()' - Do something with a defaulted string arg.
 */

int					/* O - Integer value */
default_string(int        one,		/* I - Integer */
	       const char *two = "2")	/* I - String */
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
 * 'default_int()' - Do something with a defaulted int arg.
 */

int					/* O - Integer value */
default_int(int one,			/* I - Integer */
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
 * 'void_func()' - Function taking no arguments.
 */

void
void_func(void)
{
  puts("MYNAMESPACE::void_func()");
}


/*
 * 'private_func()' - Private function.
 *
 * @private@
 */

void
private_func(void)
{
  puts("MYNAMESPACE::private_func()");
}

}
