---
title: How to Use the codedoc Utility
author: Michael R Sweet
copyright: Copyright © 2003-2022 by Michael R Sweet
version: 3.7
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
- `--language ll[-LOC]`: Sets the ISO language and locality codes; the default
  is "en-US" for US English
- `--section "{name,number}"`: Sets the section/category name (HTML and EPUB
  output) or section number (man page output)
- `--title "title text'`: Sets the title text


Markdown Extensions
-------------------

Codedoc uses the [mmd](https://www.msweet.org/mmd) library for markdown which
mostly conforms to the CommonMark version of markdown syntax with the following
exceptions:

- Embedded HTML markup and entities are explicitly not supported or allowed;
  the reason for this is to better support different kinds of output from the
  markdown "source", including XHTML, man, and `xml2rfc`.

- Tabs are silently expanded to the markdown standard of four spaces since HTML
  uses eight spaces per tab.

- Some pathological nested link and inline style features supported by
  CommonMark (`******Really Strong Text******`) are not supported.

- Support for metadata as used by Jekyll and other web markdown solutions.

- Support for `@` links which resolve to headings within the documentation.

- Support for `@@` links which resolve to literal names such as functions and
  types within the documentation.

- Support for `::WIDTHxHEIGHT`, `::WIDTHx`, and `::xHEIGHT` in image (ALT) text
  to scale images to the specified size.

- Support for the "c" and "cpp" languages for syntax highlighting in fenced
  code text.

- Support for tables as used by the
  [Github Flavored Markdown Spec](https://github.github.com/gfm).


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

The first paragraph of a comment is used as the short description of an item.
Any subsequent paragraphs are used as the detailed description of an item.


@ Directives in Comments
------------------------

Comments can also include the following special `@name ...@`
directive strings:

- `@body@`: appends the comment to the body text
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


Markdown in Comments
--------------------

Comments can use a small subset of markdown inline formatting characters:

- "\`code\`": formats the text as code
- "\*emphasized\*": emphasizes the text (usually italics)
- "\*\*strong\*\*": strongly emphasizes the text (usually boldface)
- "\[text](url)": inserts a named hyperlink
- "\<url>": inserts a hyperlink
- "\\": escapes the character that follows

In addition, example code can be surrounded by lines containing "\`\`\`":

```
/*
 * Example code:
 *
 * ```
 * foo = get_foo() + get_baz();
 * bar = get_bar() / foo;
 * if (bar > 42)
 *   do_waz();
 * ```
 */
```

Bulleted lists can be provided using a leading hyphen:

```
/*
 * Example list:
 *
 * - One fish
 * - Two fish
 * - Red fish
 * - Blue fish
 */
```

and block quotes can be provided using the ">" character:

```
/*
 * Some sort of documentation...
 *
 * > Note: This is a block quote that is typically used for special
 * > notes to the reader.
 */
```


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


CSS Classes
-----------

The following CSS classes are used to group the sections in the generated
documentation:

| Class        | Description                      | Usage                      |
| ------------ | -------------------------------- | -------------------------- |
| body         | The body of the documentation    | `<div class="body">`       |
| center       | Centered text in cell            | `<td class="center">`      |
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
| left         | Left-aligned text in cell        | `<td class="left">`        |
| list         | A table list                     | `<table class="list">`     |
| members      | A list of class/struct members   | `<table class="members">`  |
| method       | A class or structure method      | `<hN class="method">`      |
| parameters   | A list of function parameters    | `<hN class="parameters">`  |
| returnvalue  | The return value of a function   | `<hN class="returnvalue">` |
| right        | Right-aligned text in cell       | `<td class="right">`       |
| struct       | A structure                      | `<hN class="struct">`      |
| subcontents  | Second-level contents            | `<ul class="subcontents">` |
| title        | A title                          | `<hN class="title">`       |
|              | (title image)                    | `<img class="title">`      |
| typedef      | A typedef                        | `<hN class="typedef">`     |
| union        | A union                          | `<hN class="union">`       |
| variable     | A global variable                | `<hN class="variable">`    |


Default Stylesheet
------------------

```
body {
  background: white;
  color: black;
  font-family: sans-serif;
  font-size: 12pt;
}
a {
  color: black;
}
a:link, a:visited {
  color: #00f;
}
a:link:hover, a:visited:hover, a:active {
  color: #c0c;
}
body, p, h1, h2, h3, h4, h5, h6 {
  font-family: sans-serif;
  line-height: 1.4;
}
h1, h2, h3, h4, h5, h6 {
  font-weight: bold;
  page-break-inside: avoid;
}
h1 {
  font-size: 250%;
  margin: 0;
}
h2 {
  font-size: 250%;
  margin-top: 1.5em;
}
h3 {
  font-size: 200%;
  margin-bottom: 0.5em;
  margin-top: 1.5em;
}
h4 {
  font-size: 150%;
  margin-bottom: 0.5em;
  margin-top: 1.5em;
}
h5 {
  font-size: 125%;
  margin-bottom: 0.5em;
  margin-top: 1.5em;
}
h6 {
  font-size: 110%;
  margin-bottom: 0.5em;
  margin-top: 1.5em;
}
img.title {
  width: 256px;
}
div.header h1, div.header p {
  text-align: center;
}
div.contents, div.body, div.footer {
  page-break-before: always;
}
.class, .enumeration, .function, .struct, .typedef, .union {
  border-bottom: solid 2px gray;
}
.description {
  margin-top: 0.5em;
}
.function {
  margin-bottom: 0;
}
blockquote {
  border: solid thin gray;
  box-shadow: 3px 3px 5px rgba(127,127,127,0.25);
  padding: 0px 10px;
  page-break-inside: avoid;
}
p code, li code, p.code, pre, ul.code li {
  font-family: monospace;
  hyphens: manual;
  -webkit-hyphens: manual;
}
p.code, pre, ul.code li {
  background: rgba(127,127,127,0.25);
  border: thin dotted gray;
  padding: 10px;
  page-break-inside: avoid;
}
pre {
  white-space: pre-wrap;
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
h1 span.info, h2 span.info, h3 span.info, h4 span.info {
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
table {
  border-collapse: collapse;
  border-spacing: 0;
}
td {
  border: solid 1px gray;
  padding: 5px 10px;
  vertical-align: top;
}
td.left {
  text-align: left;
}
td.center {
  text-align: center;
}
td.right {
  text-align: right;
}
th {
  border-bottom: solid 2px gray;
  padding: 1px 5px;
  text-align: center;
  vertical-align: bottom;
}
tr:nth-child(even) {
  background: rgba(127,127,127,0.25);
}
table.list {
  border-collapse: collapse;
  width: 100%;
}
table.list th {
  border-bottom: none;
  border-right: 2px solid gray;
  font-family: monospace;
  font-weight: normal;
  padding: 5px 10px 5px 2px;
  text-align: right;
  vertical-align: top;
}
table.list td {
  border: none;
  padding: 5px 2px 5px 10px;
  text-align: left;
  vertical-align: top;
}
h2.title, h3.title {
  border-bottom: solid 2px gray;
}
/* Syntax highlighting */
span.comment {
  color: darkgreen;
}
span.directive {
  color: purple;
}
span.number {
  color: orange;
}
span.reserved {
  color: blue;
}
span.string {
  color: magenta;
}
/* Dark mode overrides */
@media (prefers-color-scheme: dark) {
  body {
    background: black;
    color: #ccc;
  }
  a {
    color: #ccc;
  }
  a:link, a:visited {
    color: #66f;
  }
  a:link:hover, a:visited:hover, a:active {
    color: #f06;
  }
}
```

Additional CSS for HTML Output
------------------------------

```
/* Show contents on left side in web browser */
@media screen and (min-width: 800px) {
  div.contents {
    border-right: solid thin gray;
    bottom: 0px;
    box-shadow: 3px 3px 5px rgba(127,127,127,0.5);
    font-size: 10pt;
    left: 0px;
    overflow: scroll;
    padding: 1%;
    position: fixed;
    top: 0px;
    width: 18%;
  }
  div.contents h2.title {
    margin-top: 0px;
  }
  div.header, div.body, div.footer {
    margin-left: 20%;
    padding: 1% 2%;
  }
}
/* Center title page content vertically */
@media print {
  div.header {
    padding-top: 33%;
  }
}
```
