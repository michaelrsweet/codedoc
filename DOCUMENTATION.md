---
title: How to Use the codedoc Utility
author: Michael R Sweet
copyright: Copyright © 2003-2019 by Michael R Sweet
version: 3.1
...

Introduction
============

Codedoc is a general-purpose utility which scans HTML, markdown, C, and C++
source files to produce EPUB, HTML, and man page documentation that can be read
by humans.  Unlike popular C/C++ documentation generators like Doxygen or
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
documentation for Mini-XML and [CUPS](https://www.cups.org).  It is now much
more general purpose.


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


Annotating Your C/C++ Code
==========================

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


Annotating HTML
===============

Headings
--------


Availability
------------


Annotating Markdown
===================


EPUB and HTML Stylesheets
=========================

Codedoc makes use of a CSS stylesheet for EPUB and HTML output.  The default
stylesheet should be usable for both display and printing, but you can override
it using the `--css` option, for example:

    codedoc --epub documentation.epub --css filename.css documentation.xml *.h *.c

> **Note:**
>
> Since EPUB uses XHTML (which is case-sensitive), your stylesheet should always
> use lowercase element names ("pre", not "PRE", etc.)


Classes
-------

The following HTML classes are used to group the sections in the generated
documentation:

| Class        | Description                      | Usage                      |
| ------------ | -------------------------------- | -------------------------- |
| body         | The body of the documentation    | `<div class="body">`       |
| class        | A class                          | `<hN class="class">`       |
| code         | Wrapped monospace code text      | `<p class="code">`         |
| constants    | A list of constants              | `<hN class="constants">`   |
| contents     | The table of contents            | `<div class="contents">`   |
|              | (first level)                    | `<ul class="contents">`    |
| description  | Short description                | `<p class="description">`  |
|              | (in table)                       | `<td class="description">` |
| discussion   | Additional description           | `<p class="discussion">`   |
|              | (title)                          | `<hN class="discussion">`  |
|              | (in table)                       | `<td class="discussion">`  |
| enumeration  | An enumeration type              | `<hN class="enumeration">` |
| footer       | The footer of the documentation  | `<div class="footer">`     |
| function     | A global function                | `<hN class="function">`    |
| header       | The header of the documentation  | `<div class="header">`     |
| info         | Availability information         | `<span class="info">`      |
| list         | A table list                     | `<table class="list">`     |
| members      | A list of class/struct members   | `<table class="members">`  |
| method       | A class or structure method      | `<hN class="method">`      |
| parameters   | A list of function parameters    | `<hN class="parameters">`  |
| returnvalue  | The return value of a function   | `<hN class="returnvalue">` |
| struct       | A structure                      | `<hN class="struct">`      |
| subcontents  | Second-level contents            | `<ul class="subcontents">` |
| title        | A title                          | `<hN class="title">`       |
| typedef      | A typedef                        | `<hN class="typedef">`     |
| union        | A union                          | `<hN class="union">`       |
| variable     | A global variable                | `<hN class="variable">`    |


Default Stylesheet
------------------

```
body, p, h1, h2, h3, h4 {
  font-family: sans-serif;
}
div.body h1 {
  font-size: 250%;
  font-weight: bold;
  margin: 0;
}
div.body h2 {
  font-size: 250%;
  margin-top: 1.5em;
}
div.body h3 {
  font-size: 150%;
  margin-bottom: 0.5em;
  margin-top: 1.5em;
}
div.body h4 {
  font-size: 110%;
  margin-bottom: 0.5em;
  margin-top: 1.5em;
}
div.body h5 {
  font-size: 100%;
  margin-bottom: 0.5em;
  margin-top: 1.5em;
}
div.contents {
  background: #e8e8e8;
  border: solid thin black;
  padding: 10px;
}
div.contents h1 {
  font-size: 110%;
}
div.contents h2 {
  font-size: 100%;
}
div.contents ul.contents {
  font-size: 80%;
}
.class {
  border-bottom: solid 2px gray;
}
.constants {
}
.description {
  margin-top: 0.5em;
}
.discussion {
}
.enumeration {
  border-bottom: solid 2px gray;
}
.function {
  border-bottom: solid 2px gray;
  margin-bottom: 0;
}
.members {
}
.method {
}
.parameters {
}
.returnvalue {
}
.struct {
  border-bottom: solid 2px gray;
}
.typedef {
  border-bottom: solid 2px gray;
}
.union {
  border-bottom: solid 2px gray;
}
.variable {
}
h1, h2, h3, h4, h5, h6 {
  page-break-inside: avoid;
}
blockquote {
  border: solid thin gray;
  box-shadow: 3px 3px 5px rgba(0,0,0,0.5);
  padding: 0px 10px;
  page-break-inside: avoid;
}
p code, li code, p.code, pre, ul.code li {
  background: rgba(127,127,127,0.1);
  border: thin dotted gray;
  font-family: monospace;
  font-size: 90%;
  hyphens: manual;
  -webkit-hyphens: manual;
  page-break-inside: avoid;
}
p.code, pre, ul.code li {
  padding: 10px;
}
p code, li code {
  padding: 2px 5px;
}
a:link, a:visited {
  text-decoration: none;
}
span.info {
  background: black;
  border: solid thin black;
  color: white;
  font-size: 80%;
  font-style: italic;
  font-weight: bold;
  white-space: nowrap;
}
h3 span.info, h4 span.info {
  border-top-left-radius: 10px;
  border-top-right-radius: 10px;
  float: right;
  padding: 3px 6px;
}
ul.code, ul.contents, ul.subcontents {
  list-style-type: none;
  margin: 0;
  padding-left: 0;
}
ul.code li {
  margin: 0;
}
ul.contents > li {
  margin-top: 1em;
}
ul.contents li ul.code, ul.contents li ul.subcontents {
  padding-left: 2em;
}
table.list {
  border-collapse: collapse;
  width: 100%;
}
table.list tr:nth-child(even) {
  background: rgba(127,127,127,0.1);]n"
}
table.list th {
  border-right: 2px solid gray;
  font-family: monospace;
  padding: 5px 10px 5px 2px;
  text-align: right;
  vertical-align: top;
}
table.list td {
  padding: 5px 2px 5px 10px;
  text-align: left;
  vertical-align: top;
}
h1.title {
}
h2.title {
  border-bottom: solid 2px black;
}
h3.title {
  border-bottom: solid 2px black;
}
```