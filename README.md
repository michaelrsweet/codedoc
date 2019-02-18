Codedoc v3.0
============

Codedoc is a documentation generator that scans the specified C and C++ source
files to produce an XML representation of globally accessible classes,
constants, enumerations, functions, structures, typedefs, unions, and
variables - the XML file is updated as necessary.

By default, a HTML representation of the XML file is written to the standard
output.  Codedoc also supports generating man pages and EPUB books.

Codedoc was originally bundled with the Mini-XML library as the `mxmldoc`
utility.


Changes in v3.1
---------------

- Fixed compile problems with Mini-XML v3.0.
- Added support for `id` attributes in HTML headers when generating the table
  of contents.
- Updated the markdown support with external links, additional inline markup,
  and hard line breaks.


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

To install the software, run `sudo make install`:

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

Copyright © 2003-2019 by Michael R Sweet

Codedoc is licensed under the Apache License Version 2.0.  See the files
"LICENSE" and "NOTICE" for more information.
