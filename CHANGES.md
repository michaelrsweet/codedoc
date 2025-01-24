Changes in Codedoc
==================


Changes in v3.8 (YYYY-MM-DD)
----------------------------

- Fixed bugs in the markdown parser.


Changes in v3.7 (2024-03-24)
----------------------------

- Now require Mini-XML 4.x.
- Now include a configure script.
- Added quoting of "." and "'" at the beginning of lines and now use the ".IP"
  macro instead of ".IN" in man output (Issue #12)
- Added support for `@body@` comment directive to add body text inline with code
  (Issue #10)
- Added highlighting of HTML and XML in code-fenced markdown (Issue #19)
- Added highlighting of CSS in code-fenced markdown.
- Added highlighting of reserved words, numbers, and strings in reference
  documentation, to match markdown code example highlighting.
- Fixed double-free bug (Issue #16)
- Fixed some bugs detected with fuzzing (Issue #13, Issue #14, Issue #15)
- Fixed support for markdown code fences and indentation in code example
  comments.
- Cleaned up some issues reported by Coverity and Cppcheck.


Changes in v3.6 (2020-12-31)
----------------------------

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


Changes in v3.5 (2020-10-09)
----------------------------

- Added support for C++ block comments.
- Fixed support for function types.
- Fixed support for private typedef struct/class/union constructions.
- Fixed support for markdown bullet lists.
- Fixed output of Unicode text.
- Now use the "title" class for the cover image in HTML and EPUB output.
- Now place the table of contents along the side for HTML output.
- No longer strip quoted text ('text') in comments unless the text ends with
  '()'.


Changes in v3.4 (2019-12-28)
----------------------------

- Added support for C++ namespaces (Issue #7)
- Silenced some warnings from the LGTM security scanner.


Changes in v3.3 (2019-11-17)
----------------------------

- Added basic markdown support in comments (Issue #6)
- Added a `--language` option to override the default documentation language
  "en-US".
- EPUB and HTML output now set the "generator" META content.
- Did some minor code reorganization/cleanup.


Changes in v3.2 (2019-08-28)
----------------------------

- The default HTML stylesheet no longer puts an outline box around monospaced
  text (Issue #2)
- Fixed signed character issues with fuzzer-generated "code" (Issue #3,
  Issue #4)
- Fixed a buffer overflow issue with fuzzer-generated "code" (Issue #5)
- Now use the base name of the cover image filename in HTML output.
- Fixed some markdown parsing issues.


Changes in v3.1 (2019-02-19)
----------------------------

- Fixed compile problems with Mini-XML v3.0.
- Greatly improved scanning of HTML content when generating the table of
  contents.
- Updated the markdown support with external links, additional inline markup,
  and hard line breaks.
- Copyright, trademark, and registered trademark symbols are now correctly
  mapped from their ASCII and UTF-8 representations to the output format's
  preferred encoding (Issue #1)
- Added documentation on EPUB and HTML stylesheets.


Changes in v3.0 (2019-01-04)
----------------------------

- Fixed potential crash bugs in mxmldoc found by fuzzing.
- The `--header` and `--footer` options now support markdown.
- The `mxmldoc` program now sets the EPUB subject ("Programming").
- Improved EPUB error reporting and output.
- Man page output now uses the ISO date format (yyyy-mm-dd)
- Dropped support for `--framed basename` since frame sets are deprecated in
  HTML 5.
