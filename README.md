Codedoc v1.0
============

Codedoc is a documentation generator that scans the specified C and C++ source
files to produce an XML representation of globally accessible classes,
constants, enumerations, functions, structures, typedefs, unions, and
variables - the XML file is updated as necessary.

By default, a HTML representation of the XML file is written to the standard
output.  Codedoc also supports generating man pages and EPUB books.

Codedoc was originally bundled with the Mini-XML library as the `mxmldoc`
utility.


Changes in v1.0
---------------

- Fixed a potential crash bug in mxmldoc found by fuzzing.
- The `mxmldoc` program now sets the EPUB subject ("Programming").


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

The `codedoc.1` man page provides documentation on how to use it.


Getting Help And Reporting Problems
-----------------------------------

The codedoc project page provides access to the Github issue tracking page:

    https://www.msweet.org/codedoc


Legal Stuff
-----------

Codedoc is licensed under the Apache License Version 2.0 with an exception to
allow linking against GPL2/LGPL2-only software.  See the files "LICENSE" and
"NOTICE" for more information.
