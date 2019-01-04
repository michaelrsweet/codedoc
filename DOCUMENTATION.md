---
title: How to Use the codedoc Utility
author: Michael R Sweet
copyright: Copyright © 2003-2019 by Michael R Sweet
version: 3.0
...

Introduction
============

Codedoc is a general-purpose utility which scans C and C++ source
files to produce an XML file representing the functions, types, and definitions
in those source files, as well as EPUB, HTML, and man page documentation that
can be read by humans.  Unlike popular documentation generators like Doxygen or
Javadoc, Codedoc uses in-line comments rather than comment headers, allowing for
more "natural" code documentation.  Additional markdown documentation content
can be included to make the generated documentation even more useful and
complete.

Codedoc is licensed under the Apache License Version 2.0 with an exception to
allow linking against GPL2/LGPL2-only software.  See the files "LICENSE" and
"NOTICE" in the source code archive or Git repository for more information.


History
-------

Codedoc was originally part of the [Mini-XML](https://www.msweet.org/mxml)
project (as "codedoc") and was developed specifically to generate the API
documentation for Mini-XML and [CUPS](https://www.cups.org).


Running Codedoc
===============

Codedoc produces HTML documentation by default.  For example, the following
command will scan all of the C source and header files in the current directory
and produce a HTML documentation file called "documentation.html":

    codedoc *.h *.c >documentation.html

You can also specify an XML file to create which contains all of the information
from the source files.  For example, the following command creates an XML file
called "documentation.xml" in addition to the HTML file:

    codedoc documentation.xml *.h *.c >documentation.html

The `--no-output` option disables the default HTML output:

    codedoc --no-output documentation.xml *.h *.c

You can then run codedoc again with the XML file alone to generate the HTML
documentation:

    codedoc documentation.xml >documentation.html


Creating Man Pages
------------------

The `--man name` option tells codedoc to create a man page instead of HTML
documentation, for example:

    codedoc --man libexample *.h *.c >libexample.3
    codedoc --man libexample documentation.xml *.h *.c >libexample.3
    codedoc --man libexample documentation.xml >libexample.3


Creating EPUB Books
-------------------

The `--epub filename.epub` option tells codedoc to create an EPUB book
containing the HTML documentation, for example:

    codedoc --epub documentation.epub *.h *.c
    codedoc --epub documentation.epub documentation.xml *.h *.c
    codedoc --epub documentation.epub documentation.xml


Adding Content and Metadata
---------------------------

Codedoc supports several options for adding content and metadata to the
generated documentation:

- `--author "name"`: Sets the author name
- `--body filename`: Inserts the named file between the table of contents and
  API documentation; the file can be markdown, HTML, or man source
- `--copyright "copyright text"`: Sets the copyright string
- `--coverimage filename.png`: Uses the specified cover page image file
- `--css filename.css`: Uses the specified style sheet file (HTML and EPUB
  output)
- `--footer filename`: Appends the named file after the  API documentation; the
  file can be markdown, HTML, or man source
- `--header filename`: Inserts the named file before the table of contents; the
  file can be markdown, HTML, or man source
- `--section "{name,number}"`: Sets the section/category name (HTML and EPUB
  output) or section number (man page output)
- `--title "title text'`; Sets the title text


Annotating Your Code
====================

Codedoc looks for in-line comments to describe the functions, types, and
constants in your code.  Codedoc documents all public names it finds in your
source files - any names starting with the underscore character (_)
or names that are documented with the [`@private@`](#-directives-in-comments)
directive are treated as private and are not documented.

Comments appearing directly before a function or type definition are used to
document that function or type.  Comments appearing after argument, definition,
return type, or variable declarations are used to document that argument,
definition, return type, or variable. For example, the following code excerpt
defines a key/value structure and a function that creates a new instance of that
structure:

    /* A key/value pair. This is used with the
       dictionary structure. */

    struct keyval
    {
      char *key; /* Key string */
      char *val; /* Value string */
    };

    /* Create a new key/value pair. */

    struct keyval * /* New key/value pair */
    new_keyval(
        const char *key, /* Key string */
        const char *val) /* Value string */
    {
      ...
    }

Codedoc also knows to remove extra asterisks (\*) from the comment string, so
the comment string:

    /*
     * Compute the value of PI.
     *
     * The function connects to an Internet server that streams audio of
     * mathematical monks chanting the first 100 digits of PI.
     */

will be shown as:

> Compute the value of PI.
>
> The function connects to an Internet server that streams audio of
> mathematical monks chanting the first 100 digits of PI.


@ Directives in Comments
------------------------

Comments can also include the following special `@name ...@`
directive strings:

- `@code text@`: formats the text as code
- `@deprecated@`: flags the item as deprecated to discourage its use
- `@exclude format[,...,format]@`: excludes the item from the
  documentation in the specified formats: "all" for all formats, "epub"
  for EPUB books, "html" for HTML output, and "man" for man page output
- `@link name@`: creates a hyperlink to the named function, type, etc.
- `@private@`: flags the item as private so it will not be included in the
  documentation
- `@since ...@`: flags the item as new since a particular release. The text
  following the `@since` up to the closing `@` is highlighted in the generated
  documentation, e.g. `@since libexample 1.1@`.
