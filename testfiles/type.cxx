typedef int foo_simple_t;		/* Simple integer type */

typedef int foo_simple_private_t;	/* @private@ */

typedef void *(*foo_callback_t)(const char *a, int b, void *u); /* Simple callback type */

typedef struct _foo_private_s		// Private structure
{
  int foo;				// Foo
  char *bar;				// Bar
} _foo_private_t;

static void *foo_static = NULL;		// Private data
