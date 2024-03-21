Codedoc v3.7
============

![Version](https://img.shields.io/github/v/release/michaelrsweet/codedoc?include_prereleases)
![Apache 2.0](https://img.shields.io/github/license/michaelrsweet/codedoc)

Codedoc is a general-purpose utility which scans HTML, markdown, C, and C++
source files to produce EPUB, HTML, and man page documentation that can be read
by humans.  Unlike popular C/C++ documentation generators like Doxygen or
Javadoc, Codedoc uses in-line comments rather than comment headers, allowing for
more "natural" code documentation.  Additional markdown documentation content
can be included to make the generated documentation even more useful and
complete.

Codedoc was originally bundled with the Mini-XML library as the `mxmldoc`
utility.


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

Copyright © 2003-2024 by Michael R Sweet

Codedoc is licensed under the Apache License Version 2.0.  See the files
"LICENSE" and "NOTICE" for more information.
