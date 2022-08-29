Codedoc v3.7
============

![Version](https://img.shields.io/github/v/release/michaelrsweet/codedoc?include_prereleases)
![Apache 2.0](https://img.shields.io/github/license/michaelrsweet/codedoc)
[![LGTM Grade](https://img.shields.io/lgtm/grade/cpp/github/michaelrsweet/codedoc)](https://lgtm.com/projects/g/michaelrsweet/codedoc/context:cpp)
[![LGTM Alerts](https://img.shields.io/lgtm/alerts/github/michaelrsweet/codedoc)](https://lgtm.com/projects/g/michaelrsweet/codedoc/)

Codedoc is a general-purpose utility which scans HTML, markdown, C, and C++
source files to produce EPUB, HTML, and man page documentation that can be read
by humans.  Unlike popular C/C++ documentation generators like Doxygen or
Javadoc, Codedoc uses in-line comments rather than comment headers, allowing for
more "natural" code documentation.  Additional markdown documentation content
can be included to make the generated documentation even more useful and
complete.

Codedoc was originally bundled with the Mini-XML library as the `mxmldoc`
utility.


Changes in v3.7
---------------

- Added quoting of "." and "'" at the beginning of lines and now use the ".IP"
  macro instead of ".IN" in man output (Issue #12)
- Added support for `@body@` comment directive to add body text inline with code
  (Issue #10)
- Fixed support for markdown code fences and indentation in code example
  comments.
- Cleaned up some issues reported by Coverity and Cppcheck.


Changes in v3.6
---------------

- Added support for syntax highlighting of C and C++ code.
- Added support for literal links (functions, types, etc.) using the "@@"
  target.
- Added support for `::WIDTHxHEIGHT` in image (ALT) text.
- Added support for markdown-style block quotes in comments.
- Fixed support for embedded images in EPUB output.
- Fixed some parsing issues for the public typedef - private struct design
  pattern, resulting in undocumented typedefs.
- Fixed a few Cppcheck and LGTM-detected bugs.
- No longer output unnecessary whitespace in HTML/EPUB output, for a modest
  savings in file size.


Changes in v3.5
---------------

- Added support for C++ block comments.
- Fixed support for function types.
- Fixed support for private typedef struct/class/union constructions.
- Fixed support for markdown bullet lists.
- Fixed output of Unicode text.
- Now use the "title" class for the cover image in HTML and EPUB output.
- Now place the table of contents along the side for HTML output.
- No longer strip quoted text ('text') in comments unless the text ends with
  '()'.


Changes in v3.4
---------------

- Added support for C++ namespaces (Issue #7)
- Silenced some warnings from the LGTM security scanner.


Changes in v3.3
---------------

- Added basic markdown support in comments (Issue #6)
- Added a `--language` option to override the default documentation language
  "en-US".
- EPUB and HTML output now set the "generator" META content.
- Did some minor code reorganization/cleanup.


Changes in v3.2
---------------

- The default HTML stylesheet no longer puts an outline box around monospaced
  text (Issue #2)
- Fixed signed character issues with fuzzer-generated "code" (Issue #3,
  Issue #4)
- Fixed a buffer overflow issue with fuzzer-generated "code" (Issue #5)
- Now use the base name of the cover image filename in HTML output.
- Fixed some markdown parsing issues.


Changes in v3.1
---------------

- Fixed compile problems with Mini-XML v3.0.
- Greatly improved scanning of HTML content when generating the table of
  contents.
- Updated the markdown support with external links, additional inline markup,
  and hard line breaks.
- Copyright, trademark, and registered trademark symbols are now correctly
  mapped from their ASCII and UTF-8 representations to the output format's
  preferred encoding (Issue #1)
- Added documentation on EPUB and HTML stylesheets.


Changes in v3.0
---------------

- Fixed potential crash bugs in mxmldoc found by fuzzing.
- The `--header` and `--footer` options now support markdown.
- The `mxmldoc` program now sets the EPUB subject ("Programming").
- Improved EPUB error reporting and output.
- Man page output now uses the ISO date format (yyyy-mm-dd)
- Dropped support for `--framed basename` since frame sets are deprecated in
  HTML 5.


Building Codedoc
----------------

Codedoc comes with a simple makefile that will work on most Linux/UNIX systems
and macOS, and depends on ZLIB.  Type `make` to build the software:

    make


Installing Codedoc
------------------

To install the software, run:

    sudo make install

The default install prefix is `/usr/local`, which can be overridden using the
`prefix` variable:

    sudo make install prefix=/foo


Documentation
-------------

The codedoc man page provides documentation on how to use it.  Further
documentation can be found in the file "DOCUMENTATION.md".


Getting Help And Reporting Problems
-----------------------------------

The codedoc project page provides access to the Github issue tracking page:

    https://www.msweet.org/codedoc


Legal Stuff
-----------

Copyright © 2003-2022 by Michael R Sweet

Codedoc is licensed under the Apache License Version 2.0.  See the files
"LICENSE" and "NOTICE" for more information.
