/*
 * Documentation utility for C/C++ code.
 *
 *     https://www.msweet.org/codedoc
 *
 * Copyright © 2003-2022 by Michael R Sweet.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

/*
 * Include necessary headers...
 */

#include <mxml.h>
#include "mmd.h"
#include "zipc.h"
#include <time.h>
#include <sys/stat.h>
#ifdef _WIN32
#  define gmtime_r(t,tm) gmtime_s(tm,t)
#  define localtime_r(t,tm) localtime_s(tm,t)
#else
#  include <dirent.h>
#  include <unistd.h>
#endif /* _WIN32 */


/*
 * Debug macros...
 */

//#define DEBUG 2
#ifdef DEBUG
#  define DEBUG_printf(...)     fprintf(stderr, __VA_ARGS__)
#  define DEBUG_puts(s)         fputs(s, stderr)
#else
#  define DEBUG_printf(...)
#  define DEBUG_puts(s)
#endif /* DEBUG */


/*
 * Basic states for file parser...
 */

enum
{
  STATE_NONE,				/* No state - whitespace, etc. */
  STATE_PREPROCESSOR,			/* Preprocessor directive */
  STATE_C_COMMENT,			/* Inside a C comment */
  STATE_CXX_COMMENT,			/* Inside a C++ comment */
  STATE_STRING,				/* Inside a string constant */
  STATE_CHARACTER,			/* Inside a character constant */
  STATE_IDENTIFIER,			/* Inside a keyword/identifier */
};


/*
 * Output modes...
 */

enum
{
  OUTPUT_NONE,				/* No output */
  OUTPUT_HTML,				/* Output HTML */
  OUTPUT_XML,				/* Output XML */
  OUTPUT_MAN,				/* Output nroff/man */
  OUTPUT_EPUB				/* Output EPUB (XHTML) */
};


/*
 * Highlight states...
 */

enum
{
  HIGHLIGHT_NONE,			/* No highlighting, plain text */
  HIGHLIGHT_COMMENT,			/* Comment text */
  HIGHLIGHT_COMMENT1,			/* One-line comment text */
  HIGHLIGHT_DIRECTIVE,			/* (Preprocessor) directive text */
  HIGHLIGHT_NUMBER,			/* Number text */
  HIGHLIGHT_RESERVED,			/* Reserved word text */
  HIGHLIGHT_STRING			/* String literal text */
};


/*
 * Special symbols...
 */

const char	*COPYRIGHT_ASCII = "(c)";
const int	COPYRIGHT_ASCII_LEN = 3;
const char	*COPYRIGHT_UTF8 = "\xC2\xA9";
const int	COPYRIGHT_UTF8_LEN = 2;

const char	*REGISTERED_ASCII = "(r)";
const int	REGISTERED_ASCII_LEN = 3;
const char	*REGISTERED_UTF8 = "\xC2\xAE";
const int	REGISTERED_UTF8_LEN = 2;

const char	*TRADEMARK_ASCII = "(tm)";
const int	TRADEMARK_ASCII_LEN = 4;
const char	*TRADEMARK_UTF8 = "\xE2\x84\xA2";
const int	TRADEMARK_UTF8_LEN = 3;


/*
 * Local types...
 */

typedef struct
{
  const char	*filename;		/* Filename */
  FILE		*fp;			/* File pointer */
  int		ch,			/* Saved character */
		line,			/* Current line number */
		column;			/* Current column */
} filebuf_t;

typedef struct
{
  char	buffer[65536],			/* String buffer */
	*bufptr;			/* Pointer into buffer */
} stringbuf_t;

typedef struct
{
  char	level,				/* Table of contents level (0-N) */
	anchor[64],			/* Anchor in file */
	title[447];			/* Title of section */
} toc_entry_t;

typedef struct
{
  size_t	alloc_entries,		/* Allocated entries */
                num_entries;		/* Number of entries */
  toc_entry_t	*entries;		/* Entries */
} toc_t;


/*
 * Emulate safe string functions as needed...
 */

#ifndef __APPLE__
#  define strlcat(dst,src,dstsize) codedoc_strlcat(dst,src,dstsize)
static size_t codedoc_strlcat(char *dst, const char *src, size_t dstsize)
{
  size_t dstlen, srclen;
  dstlen = strlen(dst);
  if (dstsize < (dstlen + 1))
    return (dstlen);
  dstsize -= dstlen + 1;
  if ((srclen = strlen(src)) > dstsize)
    srclen = dstsize;
  memmove(dst + dstlen, src, srclen);
  dst[dstlen + srclen] = '\0';
  return (dstlen = srclen);
}
#  define strlcpy(dst,src,dstsize) codedoc_strlcpy(dst,src,dstsize)
static size_t codedoc_strlcpy(char *dst, const char *src, size_t dstsize)
{
  size_t srclen = strlen(src);
  dstsize --;
  if (srclen > dstsize)
    srclen = dstsize;
  memmove(dst, src, srclen);
  dst[srclen] = '\0';
  return (srclen);
}
#endif /* !__APPLE__ */


/*
 * Local functions...
 */

static void		add_file_toc(toc_t *toc, const char *filename, mmd_t *file);
static void		add_toc(toc_t *toc, int level, const char *anchor, const char *title);
static mxml_node_t	*add_variable(mxml_node_t *parent, const char *name, mxml_node_t *type);
static toc_t		*build_toc(mxml_node_t *doc, const char *bodyfile, mmd_t *body, const char *footerfile, int mode);
static void		clear_whitespace(mxml_node_t *node);
static int		filebuf_getc(filebuf_t *file);
static int		filebuf_open(filebuf_t *file, const char *filename);
static void		filebuf_ungetc(filebuf_t *file, int ch);
static mxml_node_t	*find_public(mxml_node_t *node, mxml_node_t *top, const char *element, const char *name, int mode);
static void		free_toc(toc_t *toc);
static char		*get_comment_info(mxml_node_t *description);
static char		*get_iso_date(time_t t);
static mxml_node_t	*get_nth_child(mxml_node_t *node, int idx);
static const char	*get_nth_text(mxml_node_t *node, int idx, int *whitespace);
static char		*get_text(mxml_node_t *node, char *buffer, int buflen);
static void		highlight_c_string(FILE *fp, const char *s, int *histate);
static int		highlight_compare(const char **a, const char **b);
static char		*html_gets(FILE *fp, char *fragment, size_t fragsize);
static void		html_unescape(char *s);
static int		is_markdown(const char *filename);
static mxml_type_t	load_cb(mxml_node_t *node);
static const char	*markdown_anchor(const char *text);
static void		markdown_write_block(FILE *out, mmd_t *parent, int mode);
static void		markdown_write_leaf(FILE *out, mmd_t *node, int mode);
static mxml_node_t	*new_documentation(mxml_node_t **codedoc);
static void		safe_strcpy(char *dst, const char *src);
static int		scan_file(filebuf_t *file, mxml_node_t *doc, const char *nsname, mmd_t **body);
static void		sort_node(mxml_node_t *tree, mxml_node_t *func);
static int		stringbuf_append(stringbuf_t *buffer, int ch);
static void		stringbuf_clear(stringbuf_t *buffer);
static char		*stringbuf_get(stringbuf_t *buffer);
static int		stringbuf_getlast(stringbuf_t *buffer);
static size_t		stringbuf_length(stringbuf_t *buffer);
static void		update_comment(mxml_node_t *parent, mxml_node_t *comment);
static void		usage(const char *option);
static void		write_description(FILE *out, int mode, mxml_node_t *description, const char *element, int summary);
static void		write_element(FILE *out, mxml_node_t *doc, mxml_node_t *element, int mode);
static void		write_epub(const char *epubfile, const char *section, const char *title, const char *author, const char *language, const char *copyright, const char *docversion, const char *cssfile, const char *coverimage, const char *headerfile, const char *bodyfile, mmd_t *body, mxml_node_t *doc, const char *footerfile);
static void		write_file(FILE *out, const char *file, int mode);
static void		write_function(FILE *out, int mode, mxml_node_t *doc, mxml_node_t *function, int level);
static void		write_html(const char *section, const char *title, const char *author, const char *language, const char *copyright, const char *docversion, const char *cssfile, const char *coverimage, const char *headerfile, const char *bodyfile, mmd_t *body, mxml_node_t *doc, const char *footerfile);
static void		write_html_body(FILE *out, int mode, const char *bodyfile, mmd_t *body, mxml_node_t *doc);
static void		write_html_head(FILE *out, int mode, const char *section, const char *title, const char *author, const char *language, const char *copyright, const char *docversion, const char *cssfile);
static void		write_html_toc(FILE *out, const char *title, toc_t *toc, const char  *filename, const char  *target);
static void		write_man(const char *man_name, const char *section, const char *title, const char *author, const char *copyright, const char *headerfile, const char *bodyfile, mmd_t *body, mxml_node_t *doc, const char *footerfile);
static void		write_scu(FILE *out, int mode, mxml_node_t *doc, mxml_node_t *scut);
static void		write_string(FILE *out, const char *s, int mode, int len);
static const char	*ws_cb(mxml_node_t *node, int where);


/*
 * 'main()' - Main entry for test program.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line args */
     char *argv[])			/* I - Command-line args */
{
  int		i;			/* Looping var */
  int		len;			/* Length of argument */
  filebuf_t	file;			/* File to read */
  mxml_node_t	*doc = NULL;		/* XML documentation tree */
  mxml_node_t	*codedoc = NULL;	/* codedoc node */
  const char	*author = NULL,		/* Author */
		*language = NULL,	/* Language */
              	*copyright = NULL,	/* Copyright */
		*cssfile = NULL,	/* CSS stylesheet file */
		*docversion = NULL,	/* Documentation set version */
                *epubfile = NULL,	/* EPUB filename */
		*footerfile = NULL,	/* Footer file */
		*headerfile = NULL,	/* Header file */
		*bodyfile = NULL,	/* Body file */
                *coverimage = NULL,	/* Cover image file */
		*name = NULL,		/* Name of manpage */
		*section = NULL,	/* Section/keywords of documentation */
		*title = NULL,		/* Title of documentation */
		*xmlfile = NULL;	/* XML file */
  mmd_t		*body = NULL;		/* Body markdown file, if any */
  int		mode = OUTPUT_HTML,	/* Output mode */
		update = 0;		/* Updated XML file */


 /*
  * Check arguments...
  */

  for (i = 1; i < argc; i ++)
  {
    if (!strcmp(argv[i], "--help"))
    {
     /*
      * Show help...
      */

      usage(NULL);
    }
    else if (!strcmp(argv[i], "--version"))
    {
     /*
      * Show version...
      */

      puts(VERSION);
      return (0);
    }
    else if (!strcmp(argv[i], "--author") && !author)
    {
     /*
      * Set author...
      */

      i ++;
      if (i < argc)
        author = argv[i];
      else
        usage(NULL);
    }
    else if (!strcmp(argv[i], "--body") && !bodyfile)
    {
     /*
      * Set body file...
      */

      i ++;
      if (i < argc)
        bodyfile = argv[i];
      else
        usage(NULL);

      if (is_markdown(bodyfile))
        body = mmdLoad(body, bodyfile);
    }
    else if (!strcmp(argv[i], "--copyright") && !copyright)
    {
     /*
      * Set copyright...
      */

      i ++;
      if (i < argc)
        copyright = argv[i];
      else
        usage(NULL);
    }
    else if (!strcmp(argv[i], "--coverimage") && !coverimage)
    {
     /*
      * Set cover image file...
      */

      i ++;
      if (i < argc)
        coverimage = argv[i];
      else
        usage(NULL);
    }
    else if (!strcmp(argv[i], "--css") && !cssfile)
    {
     /*
      * Set CSS stylesheet file...
      */

      i ++;
      if (i < argc)
        cssfile = argv[i];
      else
        usage(NULL);
    }
    else if (!strcmp(argv[i], "--docversion") && !docversion)
    {
     /*
      * Set documentation set directory...
      */

      i ++;
      if (i < argc)
        docversion = argv[i];
      else
        usage(NULL);
    }
    else if (!strcmp(argv[i], "--epub") && !epubfile)
    {
     /*
      * Set EPUB filename...
      */

      mode = OUTPUT_EPUB;

      i ++;
      if (i < argc)
        epubfile = argv[i];
      else
        usage(NULL);
    }
    else if (!strcmp(argv[i], "--footer") && !footerfile)
    {
     /*
      * Set footer file...
      */

      i ++;
      if (i < argc)
        footerfile = argv[i];
      else
        usage(NULL);
    }
    else if (!strcmp(argv[i], "--header") && !headerfile)
    {
     /*
      * Set header file...
      */

      i ++;
      if (i < argc)
        headerfile = argv[i];
      else
        usage(NULL);
    }
    else if (!strcmp(argv[i], "--language") && !language)
    {
     /*
      * Set language...
      */

      i ++;
      if (i < argc)
        language = argv[i];
      else
        usage(NULL);
    }
    else if (!strcmp(argv[i], "--man") && !name)
    {
     /*
      * Output manpage...
      */

      i ++;
      if (i < argc)
      {
        mode = OUTPUT_MAN;
        name = argv[i];
      }
      else
        usage(NULL);
    }
    else if (!strcmp(argv[i], "--no-output"))
      mode = OUTPUT_NONE;
    else if (!strcmp(argv[i], "--section") && !section)
    {
     /*
      * Set section/keywords...
      */

      i ++;
      if (i < argc)
        section = argv[i];
      else
        usage(NULL);
    }
    else if (!strcmp(argv[i], "--title") && !title)
    {
     /*
      * Set title...
      */

      i ++;
      if (i < argc)
        title = argv[i];
      else
        usage(NULL);
    }
    else if (argv[i][0] == '-')
    {
     /*
      * Unknown/bad option...
      */

      usage(argv[i]);
    }
    else
    {
     /*
      * Process XML or source file...
      */

      len = (int)strlen(argv[i]);
      if (len > 4 && !strcmp(argv[i] + len - 4, ".xml"))
      {
       /*
        * Set XML file...
	*/

        if (xmlfile)
	  usage(NULL);

        xmlfile = argv[i];

        if (!doc)
	{
	  FILE *fp = fopen(argv[i], "r");

	  if (fp)
	  {
	   /*
	    * Read the existing XML file...
	    */

	    doc = mxmlLoadFile(NULL, fp, load_cb);

	    fclose(fp);

	    if (!doc)
	    {
	      codedoc = NULL;

	      fprintf(stderr, "codedoc: Unable to read the XML documentation file \"%s\".\n", argv[i]);
	    }
	    else if ((codedoc = mxmlFindElement(doc, doc, "codedoc", NULL, NULL, MXML_DESCEND)) == NULL)
	    {
	      fprintf(stderr, "codedoc: XML documentation file \"%s\" is missing the <codedoc> node.\n", argv[i]);

	      mxmlDelete(doc);
	      doc = NULL;
	    }
	  }
	  else
	  {
	    doc     = NULL;
	    codedoc = NULL;
	  }

	  if (!doc)
	    doc = new_documentation(&codedoc);
        }
      }
      else
      {
       /*
        * Load source file...
	*/

        update = 1;

	if (!doc)
	  doc = new_documentation(&codedoc);

        if (!filebuf_open(&file, argv[i]))
        {
	  mxmlDelete(doc);
	  return (1);
	}
	else if (!scan_file(&file, codedoc, NULL, &body))
	{
	  fclose(file.fp);
	  mxmlDelete(doc);
	  return (1);
	}
	else
	  fclose(file.fp);
      }
    }
  }

  if (update && xmlfile)
  {
   /*
    * Save the updated XML documentation file...
    */

    FILE *fp = fopen(xmlfile, "w");

    if (fp)
    {
     /*
      * Write over the existing XML file...
      */

      mxmlSetWrapMargin(0);

      if (mxmlSaveFile(doc, fp, ws_cb))
      {
	fprintf(stderr, "codedoc: Unable to write the XML documentation file \"%s\": %s\n", xmlfile, strerror(errno));
	fclose(fp);
	mxmlDelete(doc);
	return (1);
      }

      fclose(fp);
    }
    else
    {
      fprintf(stderr, "codedoc: Unable to create the XML documentation file \"%s\": %s\n", xmlfile, strerror(errno));
      mxmlDelete(doc);
      return (1);
    }
  }

 /*
  * Collect the default metadata values, if present.
  */

  if (!title)
    title = mmdGetMetadata(body, "title");
  if (!title)
    title = "Documentation";

  if (!author)
    author = mmdGetMetadata(body, "author");
  if (!author)
    author = "Unknown";

  if (!language)
    language = mmdGetMetadata(body, "language");
  if (!language)
    language = "en-US";

  if (!copyright)
    copyright = mmdGetMetadata(body, "copyright");
  if (!copyright)
    copyright = "Unknown";

  if (!docversion)
    docversion = mmdGetMetadata(body, "version");
  if (!docversion)
    docversion = "0.0";

 /*
  * Write output...
  */

  switch (mode)
  {
    case OUTPUT_EPUB :
       /*
        * Write EPUB (XHTML) documentation...
        */

        write_epub(epubfile, section, title, author, language, copyright, docversion, cssfile, coverimage, headerfile, bodyfile, body, codedoc, footerfile);
        break;

    case OUTPUT_HTML :
       /*
        * Write HTML documentation...
        */

        write_html(section, title, author, language, copyright, docversion, cssfile, coverimage, headerfile, bodyfile, body, codedoc, footerfile);
        break;

    case OUTPUT_MAN :
       /*
        * Write manpage documentation...
        */

        write_man(name, section, title, author, copyright, headerfile, bodyfile, body, codedoc, footerfile);
        break;
  }

  if (body)
    mmdFree(body);

 /*
  * Delete the tree and return...
  */

  mxmlDelete(doc);

  return (0);
}


/*
 * 'add_file_toc()' - Add TOC entries from a file.
 */

static void
add_file_toc(toc_t      *toc,		/* I - Table-of-contents */
             const char *filename,	/* I - Filename */
             mmd_t      *file)		/* I - Markdown document */
{
  FILE		*fp;			/* File pointer */


  if (file)
  {
    mmd_t	*node,			/* Current node */
		*tnode,			/* Title node */
		*next;			/* Next node */
    mmd_type_t	type;			/* Node type */
    char	title[1024],		/* Heading title */
		*ptr;			/* Pointer into title */

    for (node = mmdGetFirstChild(file); node; node = next)
    {
      type = mmdGetType(node);

      if (type == MMD_TYPE_HEADING_1 || type == MMD_TYPE_HEADING_2)
      {
        title[sizeof(title) - 1] = '\0';

        for (tnode = mmdGetFirstChild(node), ptr = title; tnode; tnode = mmdGetNextSibling(tnode))
        {
          if (mmdGetWhitespace(tnode) && ptr < (title + sizeof(title) - 1))
            *ptr++ = ' ';

          strncpy(ptr, mmdGetText(tnode), sizeof(title) - (ptr - title) - 1);
          ptr += strlen(ptr);
        }

        add_toc(toc, type - MMD_TYPE_HEADING_1 + 1, markdown_anchor(title), title);
      }

      if ((next = mmdGetNextSibling(node)) == NULL)
      {
        next = mmdGetParent(node);

        while (next && mmdGetNextSibling(next) == NULL)
          next = mmdGetParent(next);

        next = mmdGetNextSibling(next);
      }
    }
  }
  else if (filename && (fp = fopen(filename, "r")) != NULL)
  {
    char	fragment[8192],		/* Fragment from file */
		endfrag[6],		/* End fragment */
		lowlevel = '6',		/* Lowest heading level seen */
		title[8192],		/* Title for heading */
		*titleptr,		/* Pointer into title */
		*titleend,		/* End of title */
		anchor[256];		/* Anchor for heading */
    int		level;			/* New heading level */
    size_t	fraglen;		/* Length of fragment */


    while (html_gets(fp, fragment, sizeof(fragment)))
    {
     /*
      * See if this is a heading...
      */

      if (strncasecmp(fragment, "<h1 ", 4) && strcasecmp(fragment, "<h1>") && strncasecmp(fragment, "<h2 ", 4) && strcasecmp(fragment, "<h2>") && strncasecmp(fragment, "<h3 ", 4) && strcasecmp(fragment, "<h3>"))
        continue;

     /*
      * Yes, prep and get the title...
      */

      if (fragment[2] < lowlevel)
        lowlevel = fragment[2];

      level     = fragment[2] - lowlevel + 1;
      title[0]  = '\0';
      titleptr  = title;
      titleend  = title + sizeof(title) - 1;
      *titleend = '\0';

      snprintf(endfrag, sizeof(endfrag), "</h%d>", level + 1);

      zipcXMLGetAttribute(fragment, "id", anchor, sizeof(anchor));

      while (html_gets(fp, fragment, sizeof(fragment)))
      {
        if (!strcasecmp(fragment, endfrag))
        {
         /*
          * End of heading...
          */
          break;
        }
        else if (!strncasecmp(fragment, "<a ", 3) && !anchor[0])
        {
         /*
          * Possible <a name="foo">...
          */

          if (!zipcXMLGetAttribute(fragment, "id", anchor, sizeof(anchor)))
            zipcXMLGetAttribute(fragment, "name", anchor, sizeof(anchor));
        }
        else if (!strncasecmp(fragment, "<span ", 6))
        {
          char	temp[256];		/* Temporary string */

          if (zipcXMLGetAttribute(fragment, "class", temp, sizeof(temp)) && !strcasecmp(temp, "info"))
          {
           /*
            * Skip informational annotation...
            */

	    while (html_gets(fp, fragment, sizeof(fragment)))
	      if (!strcasecmp(fragment, "</span>"))
	        break;
          }
        }
        else if (fragment[0] != '<')
        {
          if ((fraglen = strlen(fragment)) > (titleend - titleptr))
            fraglen = titleend - titleptr;

          strncpy(titleptr, fragment, fraglen);
          titleptr += fraglen;
          *titleptr = '\0';
        }
      }

     /*
      * Convert newlines and tabs to spaces, then unescape HTML entities...
      */

      for (titleptr = title; *titleptr; titleptr ++)
        if (isspace(*titleptr & 255))
	  *titleptr = ' ';

      html_unescape(title);

     /*
      * If both the title and anchor are not empty, add the table-of-contents
      * entry...
      */

      if (anchor[0] && title[0])
        add_toc(toc, level, anchor, title);
    }

    fclose(fp);
  }
}


/*
 * 'add_toc()' - Add a TOC entry.
 */

static void
add_toc(toc_t      *toc,		/* I - Table-of-contents */
        int        level,		/* I - Level (1-N) */
        const char *anchor,		/* I - Anchor */
        const char *title)		/* I - Title */
{
  toc_entry_t	*temp;			/* New pointer */


  if (toc->num_entries >= toc->alloc_entries)
  {
    toc->alloc_entries += 100;
    if (!toc->entries)
      temp = malloc(sizeof(toc_entry_t) * toc->alloc_entries);
    else
      temp = realloc(toc->entries, sizeof(toc_entry_t) * toc->alloc_entries);

    if (!temp)
      return;

    toc->entries = temp;
  }

  temp = toc->entries + toc->num_entries;
  toc->num_entries ++;

  temp->level = level;
  strlcpy(temp->anchor, anchor, sizeof(temp->anchor));
  strlcpy(temp->title, title, sizeof(temp->title));
}


/*
 * 'add_variable()' - Add a variable or argument.
 */

static mxml_node_t *			/* O - New variable/argument */
add_variable(mxml_node_t *parent,	/* I - Parent node */
             const char  *name,		/* I - "argument" or "variable" */
             mxml_node_t *type)		/* I - Type nodes */
{
  mxml_node_t	*variable,		/* New variable */
		*node,			/* Current node */
		*next;			/* Next node */
  char		buffer[16384],		/* String buffer */
		*bufptr;		/* Pointer into buffer */
  int		whitespace = 0;		/* Whitespace before string? */
  const char	*string = NULL;		/* String value */


  DEBUG_printf("add_variable(parent=%p, name=\"%s\", type=%p)\n", parent, name, type);

 /*
  * Range check input...
  */

  if (!type || !mxmlGetFirstChild(type))
    return (NULL);

 /*
  * Create the variable/argument node...
  */

  variable = mxmlNewElement(parent, name);

 /*
  * Check for a default value...
  */

  for (node = mxmlGetFirstChild(type); node; node = mxmlGetNextSibling(node))
  {
    string = mxmlGetText(node, &whitespace);

    if (!strcmp(string, "="))
      break;
  }

  if (node)
  {
   /*
    * Default value found, copy it and add as a "default" attribute...
    */

    for (bufptr = buffer; node; bufptr += strlen(bufptr))
    {
      string = mxmlGetText(node, &whitespace);

      if (whitespace && bufptr > buffer && bufptr < (buffer + sizeof(buffer) - 1))
	*bufptr++ = ' ';

      strlcpy(bufptr, string, sizeof(buffer) - (size_t)(bufptr - buffer));

      next = mxmlGetNextSibling(node);
      mxmlDelete(node);
      node = next;
    }

    mxmlElementSetAttr(variable, "default", buffer);
  }

 /*
  * Extract the argument/variable name...
  */

  string = mxmlGetText(mxmlGetLastChild(type), &whitespace);

  if (*string == ')')
  {
   /*
    * Handle "type (*name)(args)"...
    */

    for (node = mxmlGetFirstChild(type); node; node = mxmlGetNextSibling(node))
    {
      string = mxmlGetText(node, &whitespace);
      if (*string == '(')
	break;
    }

    for (bufptr = buffer; node; bufptr += strlen(bufptr))
    {
      string = mxmlGetText(node, &whitespace);

      if (whitespace && bufptr > buffer)
	*bufptr++ = ' ';

      strlcpy(bufptr, string, sizeof(buffer) - (size_t)(bufptr - buffer));

      next = mxmlGetNextSibling(node);
      mxmlDelete(node);
      node = next;
    }
  }
  else
  {
   /*
    * Handle "type name"...
    */

    strlcpy(buffer, string, sizeof(buffer));
    mxmlDelete(mxmlGetLastChild(type));
  }

 /*
  * Set the name...
  */

  mxmlElementSetAttr(variable, "name", buffer);

 /*
  * Add the remaining type information to the variable node...
  */

  mxmlAdd(variable, MXML_ADD_AFTER, MXML_ADD_TO_PARENT, type);

 /*
  * Add new new variable node...
  */

  return (variable);
}


/*
 * 'build_toc()' - Build a table-of-contents...
 */

static toc_t *				/* O - Table of contents */
build_toc(mxml_node_t *doc,		/* I - Documentation */
          const char  *bodyfile,	/* I - Body file */
          mmd_t       *body,		/* I - Markdown body */
          const char  *footerfile,	/* I - Footer file */
          int         mode)             /* I - Output mode */
{
  toc_t		*toc;			/* Array of headings */
  mxml_node_t	*function,		/* Current function */
		*scut,			/* Struct/class/union/typedef */
		*arg;			/* Current argument */
  const char	*name;			/* Name of function/type */


 /*
  * Make a new table-of-contents...
  */

  if ((toc = calloc(1, sizeof(toc_t))) == NULL)
    return (NULL);

 /*
  * Scan the body file for headings...
  */

  add_file_toc(toc, bodyfile, body);

 /*
  * Next the classes...
  */

  if ((scut = find_public(doc, doc, "class", NULL, mode)) != NULL)
  {
    add_toc(toc, 1, "CLASSES", "Classes");

    while (scut)
    {
      name = mxmlElementGetAttr(scut, "name");
      scut = find_public(scut, doc, "class", NULL, mode);
      add_toc(toc, 2, name, name);
    }
  }

 /*
  * Functions...
  */

  if ((function = find_public(doc, doc, "function", NULL, mode)) != NULL)
  {
    add_toc(toc, 1, "FUNCTIONS", "Functions");

    while (function)
    {
      name     = mxmlElementGetAttr(function, "name");
      function = find_public(function, doc, "function", NULL, mode);
      add_toc(toc, 2, name, name);
    }
  }

 /*
  * Data types...
  */

  if ((scut = find_public(doc, doc, "typedef", NULL, mode)) != NULL)
  {
    add_toc(toc, 1, "TYPES", "Data Types");

    while (scut)
    {
      name = mxmlElementGetAttr(scut, "name");
      scut = find_public(scut, doc, "typedef", NULL, mode);
      add_toc(toc, 2, name, name);
    }
  }

 /*
  * Structures...
  */

  if ((scut = find_public(doc, doc, "struct", NULL, mode)) != NULL)
  {
    add_toc(toc, 1, "STRUCTURES", "Structures");

    while (scut)
    {
      name = mxmlElementGetAttr(scut, "name");
      scut = find_public(scut, doc, "struct", NULL, mode);
      add_toc(toc, 2, name, name);
    }
  }

 /*
  * Unions...
  */

  if ((scut = find_public(doc, doc, "union", NULL, mode)) != NULL)
  {
    add_toc(toc, 1, "UNIONS", "Unions");

    while (scut)
    {
      name = mxmlElementGetAttr(scut, "name");
      scut = find_public(scut, doc, "union", NULL, mode);
      add_toc(toc, 2, name, name);
    }
  }

 /*
  * Globals variables...
  */

  if ((arg = find_public(doc, doc, "variable", NULL, mode)) != NULL)
  {
    add_toc(toc, 1, "VARIABLES", "Variables");

    while (arg)
    {
      name = mxmlElementGetAttr(arg, "name");
      arg = find_public(arg, doc, "variable", NULL, mode);
      add_toc(toc, 2, name, name);
    }
  }

 /*
  * Enumerations/constants...
  */

  if ((scut = find_public(doc, doc, "enumeration", NULL, mode)) != NULL)
  {
    add_toc(toc, 1, "ENUMERATIONS", "Enumerations");

    while (scut)
    {
      name = mxmlElementGetAttr(scut, "name");
      scut = find_public(scut, doc, "enumeration", NULL, mode);
      add_toc(toc, 2, name, name);
    }
  }

 /*
  * Scan the footer file for headings...
  */

  if (footerfile)
  {
    mmd_t *mmd = NULL;

    if (is_markdown(footerfile))
      mmd = mmdLoad(NULL, footerfile);

    add_file_toc(toc, footerfile, mmd);

    mmdFree(mmd);
  }

  return (toc);
}


/*
 * 'clear_whitespace()' - Clear the whitespace value of a text node.
 */

static void
clear_whitespace(mxml_node_t *node)	/* I - Node */
{
 /*
  * Mini-XML 2.x has a bug (Issue #241) where setting the value of a node to
  * the same pointer will cause a use-after-free error.  Right now we detect at
  * compile time whether we need to use this workaround...
  */

#if MXML_MAJOR_VERSION < 3
  char *s = strdup(mxmlGetText(node, NULL));
  mxmlSetText(node, 0, s);
  free(s);

#else
  mxmlSetText(node, 0, mxmlGetText(node, NULL));
#endif /* MXML_MAJOR_VERSION < 3 */
}


/*
 * 'epub_ws_cb()' - Whitespace callback for EPUB.
 */

static const char *			/* O - Whitespace string or NULL for none */
epub_ws_cb(mxml_node_t *node,		/* I - Element node */
           int         where)		/* I - Where value */
{
  mxml_node_t	*temp;			/* Temporary node pointer */
  int		depth;			/* Depth of node */
  static const char *spaces = "                                        ";
					/* Whitespace (40 spaces) for indent */


  switch (where)
  {
    case MXML_WS_BEFORE_CLOSE :
        if ((temp = mxmlGetFirstChild(node)) != NULL && mxmlGetType(temp) != MXML_ELEMENT)
          return (NULL);

	for (depth = -4; node; node = mxmlGetParent(node), depth += 2)
	  ;				// Count the depth

	if (depth > 40)
	  return (spaces);
	else if (depth < 2)
	  return (NULL);
	else
	  return (spaces + 40 - depth);

    case MXML_WS_AFTER_CLOSE :
	return ("\n");

    case MXML_WS_BEFORE_OPEN :
	for (depth = -4; node; node = mxmlGetParent(node), depth += 2)
	  ;				// Count the depth

	if (depth > 40)
	  return (spaces);
	else if (depth < 2)
	  return (NULL);
	else
	  return (spaces + 40 - depth);

    default :
    case MXML_WS_AFTER_OPEN :
        if ((temp = mxmlGetFirstChild(node)) != NULL && mxmlGetType(temp) != MXML_ELEMENT)
          return (NULL);

        return ("\n");
  }
}


/*
 * 'filebuf_getc()' - Get a UTF-8 character from a file, tracking the line and
 *                    column.
 */

static int				/* O - Character or EOF */
filebuf_getc(filebuf_t *file)		/* I - File buffer */
{
  int	ch;				/* Current character */


  if (file->ch)
  {
    if (file->ch > 0x100)
    {
      ch       = (file->ch >> 8) & 255;
      file->ch = file->ch & 255;
    }
    else
    {
      ch       = file->ch;
      file->ch = 0;
    }

    return (ch);
  }

  if ((ch = getc(file->fp)) == EOF)
    return (EOF);

  if (ch & 0x80)
  {
    if ((ch & 0xe0) == 0xc0)
    {
      int ch2 = getc(file->fp);

      if ((ch2 & 0xc0) != 0x80)
        goto bad_utf8;

      ch = ((ch & 0x1f) << 6) | (ch2 & 0x3f);
    }
    else if ((ch & 0xf0) == 0xe0)
    {
      int ch2 = getc(file->fp);
      int ch3 = getc(file->fp);

      if ((ch2 & 0xc0) != 0x80 || (ch3 & 0xc0) != 0x80)
        goto bad_utf8;

      ch = ((ch & 0x0f) << 12) | ((ch2 & 0x3f) << 6) | (ch3 & 0x3f);
    }
    else if ((ch & 0xf8) == 0xf0)
    {
      int ch2 = getc(file->fp);
      int ch3 = getc(file->fp);
      int ch4 = getc(file->fp);

      if ((ch2 & 0xc0) != 0x80 || (ch3 & 0xc0) != 0x80 || (ch4 & 0xc0) != 0x80)
        goto bad_utf8;

      ch = ((ch & 0x07) << 18) | ((ch2 & 0x3f) << 12) | ((ch3 & 0x3f) << 6) | (ch4 & 0x3f);
    }
    else
      goto bad_utf8;
  }

  if (ch == 0x7f || ch < 0x07 || ch == 0x08 || (ch > 0x0d && ch < ' '))
  {
    fprintf(stderr, "%s:%d(%d) Illegal control character found.\n", file->filename, file->line, file->column);
    exit(1);
  }

  if (ch == 0x09)
  {
   /*
    * Tab...  Traditional tabs are 8 columns...
    */

    file->column = ((file->column + 7) & ~7) + 1;
  }
  else if (ch == 0x0a || ch == 0x0c)
  {
   /*
    * Line feed and form feed...
    */

    file->line ++;
    file->column = 1;
  }
  else if (ch == 0x0b)
  {
   /*
    * Vertical tab...
    */

    file->line ++;
  }
  else if (ch == 0x0d)
  {
   /*
    * Carriage return...
    */

    file->column = 1;
  }
  else
    file->column ++;

  return (ch);

 /*
  * If we get here, the UTF-8 sequence is bad...
  */

  bad_utf8:

  fprintf(stderr, "%s:%d(%d) Illegal UTF-8 sequence found.\n", file->filename, file->line, file->column);
  exit(1);
}


/*
 * 'filebuf_open()' - Open a file.
 */

static int				/* O - 1 on success, 0 on failure */
filebuf_open(filebuf_t  *file,		/* I - File buffer */
             const char *filename)	/* I - Filename to open */
{
  file->filename = filename;
  file->fp       = fopen(filename, "rb");
  file->ch       = 0;
  file->line     = 1;
  file->column   = 1;

  if (!file->fp)
    perror(filename);

  return (file->fp != NULL);
}


/*
 * 'filebuf_ungetc()' - Save the previous character read from a file.
 */

static void
filebuf_ungetc(filebuf_t *file,		/* I - File buffer */
               int       ch)		/* I - Character to save */
{
  file->ch = ch;
}


/*
 * 'find_public()' - Find a public function, type, etc.
 */

static mxml_node_t *			/* I - Found node or NULL */
find_public(mxml_node_t *node,		/* I - Current node */
            mxml_node_t *top,		/* I - Top node */
            const char  *element,	/* I - Element */
            const char  *name,		/* I - Name */
            int         mode)           /* I - Output mode */
{
  mxml_node_t	*description,		/* Description node */
		*comment;		/* Comment node */


  for (node = mxmlFindElement(node, top, element, name ? "name" : NULL, name, node == top ? MXML_DESCEND_FIRST : MXML_NO_DESCEND);
       node;
       node = mxmlFindElement(node, top, element, name ? "name" : NULL, name, MXML_NO_DESCEND))
  {
   /*
    * Get the description for this node...
    */

    description = mxmlFindElement(node, node, "description", NULL, NULL, MXML_DESCEND_FIRST);

   /*
    * A missing or empty description signals a private node...
    */

    if (!description)
      continue;

   /*
    * Look for @private@ or @exclude format@ in the comment text...
    */

    for (comment = mxmlGetFirstChild(description); comment; comment = mxmlGetNextSibling(comment))
    {
      const char *s = mxmlGetType(comment) == MXML_TEXT ? mxmlGetText(comment, NULL) : mxmlGetOpaque(comment);
      const char *exclude;

     /*
      * Skip anything marked private...
      */

      if (strstr(s, "@private@"))
        break;

     /*
      * Skip items excluded for certain formats...
      */

      if ((exclude = strstr(s, "@exclude ")) != NULL)
      {
        exclude += 9;

        if (!strncmp(exclude, "all@", 4))
        {
          break;
        }
        else
        {
          while (*exclude != '@')
          {
            if (!strncmp(exclude, "docset", 6))
            {
              /* Legacy, no longer supported */
              exclude += 6;
            }
            else if (!strncmp(exclude, "epub", 4))
            {
              if (mode == OUTPUT_EPUB)
                break;
              exclude += 4;
            }
            else if (!strncmp(exclude, "html", 4))
            {
              if (mode == OUTPUT_HTML)
                break;
              exclude += 4;
            }
            else if (!strncmp(exclude, "man", 3))
            {
              if (mode == OUTPUT_MAN)
                break;
              exclude += 3;
            }
            else if (!strncmp(exclude, "tokens", 6))
            {
              /* Legacy, no longer supported */
              exclude += 6;
            }
            else if (!strncmp(exclude, "xml", 3))
            {
              if (mode == OUTPUT_XML)
                break;
              exclude += 3;
            }
            else
              break;

            if (*exclude == ',')
              exclude ++;
            else if (*exclude != '@')
              break;
          }

          if (*exclude != '@')
            break;
        }
      }
    }

    if (!comment)
    {
     /*
      * No @private@, so return this node...
      */

      return (node);
    }
  }

 /*
  * If we get here, there are no (more) public nodes...
  */

  return (NULL);
}


/*
 * 'free_toc()' - Free a table-of-contents.
 */

static void
free_toc(toc_t *toc)			/* I - Table of contents */
{
  free(toc->entries);
  free(toc);
}


/*
 * 'get_comment_info()' - Get info from comment.
 */

static char *				/* O - Info from comment */
get_comment_info(
    mxml_node_t *description)		/* I - Description node */
{
  char		text[10240],		/* Description text */
		since[255],		/* @since value */
		*ptr;			/* Pointer into text */
  static char	info[1024];		/* Info string */


  if (!description)
    return ("");

  get_text(description, text, sizeof(text));

  for (ptr = strchr(text, '@'); ptr; ptr = strchr(ptr + 1, '@'))
  {
    if (!strncmp(ptr, "@deprecated@", 12))
      return ("<span class=\"info\">&#160;DEPRECATED&#160;</span>");
    else if (!strncmp(ptr, "@since ", 7))
    {
      strlcpy(since, ptr + 7, sizeof(since));

      if ((ptr = strchr(since, '@')) != NULL)
        *ptr = '\0';

      snprintf(info, sizeof(info), "<span class=\"info\">&#160;%s&#160;</span>", since);
      return (info);
    }
  }

  return ("");
}


/*
 * 'get_iso_date()' - Get an ISO-formatted date/time string.
 */

static char *				/* O - ISO date/time string */
get_iso_date(time_t t)			/* I - Time value */
{
  struct tm	date;			/* UTC date/time */
  static char	buffer[100];		/* String buffer */


  gmtime_r(&t, &date);

  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02dZ", date.tm_year + 1900, date.tm_mon + 1, date.tm_mday, date.tm_hour, date.tm_min, date.tm_sec);

  return (buffer);
}


/*
 * 'get_nth_child()' - Get the Nth child node.
 */

static mxml_node_t *			/* O - Child node or NULL */
get_nth_child(mxml_node_t *node,	/* I - Parent node */
              int         idx)		/* I - Child node index (negative to index from end) */
{
  if (idx < 0)
  {
    for (node = mxmlGetLastChild(node); node && idx < -1; idx ++)
      node = mxmlGetPrevSibling(node);
  }
  else
  {
    for (node = mxmlGetFirstChild(node); node && idx > 0; idx --)
      node = mxmlGetNextSibling(node);
  }

  return (node);
}


/*
 * 'get_nth_text()' - Get the text string from the Nth child node.
 */

static const char *			/* O - String value or NULL */
get_nth_text(mxml_node_t *node,		/* I - Parent node */
             int         idx,		/* I - Child node index (negative to index from end) */
             int         *whitespace)	/* O - Whitespace value (NULL for don't care) */
{
  return (mxmlGetText(get_nth_child(node, idx), whitespace));
}


/*
 * 'get_text()' - Get the text for a node.
 */

static char *				/* O - Text in node */
get_text(mxml_node_t *node,		/* I - Node to get */
         char        *buffer,		/* I - Buffer */
	 int         buflen)		/* I - Size of buffer */
{
  char		*ptr,			/* Pointer into buffer */
		*end;			/* End of buffer */
  size_t	len;			/* Length of node */
  mxml_node_t	*current;		/* Current node */


  ptr = buffer;
  end = buffer + buflen - 1;

  for (current = mxmlGetFirstChild(node); current && ptr < end; current = mxmlGetNextSibling(current))
  {
    mxml_type_t type = mxmlGetType(current);

    if (type == MXML_TEXT)
    {
      int whitespace;
      const char *string = mxmlGetText(current, &whitespace);

      if (whitespace)
        *ptr++ = ' ';

      len = strlen(string);
      if (len > (size_t)(end - ptr))
        len = (size_t)(end - ptr);

      memcpy(ptr, string, len);
      ptr += len;
    }
    else if (type == MXML_OPAQUE)
    {
      const char *opaque = mxmlGetOpaque(current);

      len = strlen(opaque);
      if (len > (size_t)(end - ptr))
        len = (size_t)(end - ptr);

      memcpy(ptr, opaque, len);
      ptr += len;
    }
  }

  *ptr = '\0';

  return (buffer);
}


/*
 * 'highlight_c_string()' - Output a string of C code, highlighting it as needed.
 */

static void
highlight_c_string(FILE       *fp,	/* I  - Output file */
                   const char *s,	/* I  - String */
                   int        *histate)	/* IO - Highlighting state */
{
  const char	*start = s,		/* Start of code to highlight */
		*class_name = (*histate == HIGHLIGHT_COMMENT) ? "comment" : NULL;
					/* Class name for current fragment */
  char		keyword[32],		/* Current keyword */
		*keyptr = keyword;	/* Pointer into keyword */
  static const char * const words[] =	/* Reserved words */
  {
    "and", "and_eq", "asm", "auto", "bitand", "bitor", "bool", "break",
    "case", "catch", "char", "class", "compl", "const", "const_cast",
    "continue", "default", "delete", "do", "double", "dynamic_cast",
    "else", "enum", "explicit", "extern", "false", "float", "for",
    "friend", "goto", "if", "inline", "int", "long", "mutable",
    "namespace", "new", "not", "not_eq", "operator", "or", "or_eq",
    "private", "protected", "public", "register", "reinterpret_cast",
    "return", "short", "signed", "sizeof", "static", "static_cast",
    "struct", "switch", "template", "this", "throw", "true", "try",
    "typedef", "typename", "union", "unsigned", "virtual", "void",
    "volatile", "while", "xor", "xor_eq"
  };


  if (*histate == HIGHLIGHT_COMMENT)
  {
    if ((s = strstr(start, "*/")) != NULL)
    {
     /*
      * Comment ends on this line...
      */

      s += 2;

      fputs("<span class=\"comment\">", fp);
      write_string(fp, start, OUTPUT_HTML, s - start);
      fputs("</span>", fp);

      start      = s;
      *histate   = HIGHLIGHT_NONE;
      class_name = NULL;
    }
    else
    {
     /*
      * Comment continues beyond the current line...
      */

      s = start + strlen(start) - 1;
    }
  }
  else if (*s == '#')
  {
   /*
    * Preprocessor directive...
    */

    while (*s && *s != '\n')
    {
      if (!strncmp(s, "/*", 2) || !strncmp(s, "//", 2))
        break;

      s ++;
    }

    fputs("<span class=\"directive\">", fp);
    write_string(fp, start, OUTPUT_HTML, s - start);
    fputs("</span>", fp);

    start = s;
  }

  while (*s && *s != '\n')
  {
    if (!strncmp(s, "/*", 2))
    {
     /*
      * Start of a block comment...
      */

      if (s > start)
      {
       /*
        * Output current fragment...
        */

	if (class_name)
	{
	  fprintf(fp, "<span class=\"%s\">", class_name);
	  write_string(fp, start, OUTPUT_HTML, s - start);
	  fputs("</span>", fp);
	}
	else
	{
	  write_string(fp, start, OUTPUT_HTML, s - start);
	}

	start = s;
      }

     /*
      * At this point, "start" points to the start of the comment...
      */

      if ((s = strstr(start, "*/")) != NULL)
      {
       /*
        * Comment ends on the current line...
        */

        s += 2;

	fputs("<span class=\"comment\">", fp);
	write_string(fp, start, OUTPUT_HTML, s - start);
	fputs("</span>", fp);

	start      = s;
	*histate   = HIGHLIGHT_NONE;
	class_name = NULL;
      }
      else
      {
       /*
        * Comment continues to the next line...
        */

	s          = start + strlen(start) - 1;
	*histate   = HIGHLIGHT_COMMENT;
	class_name = "comment";
	break;
      }
    }
    else if (!strncmp(s, "//", 2))
    {
     /*
      * Start of C++ comment...
      */

      if (s > start)
      {
       /*
        * Output current fragment...
        */

	if (class_name)
	{
	  fprintf(fp, "<span class=\"%s\">", class_name);
	  write_string(fp, start, OUTPUT_HTML, s - start);
	  fputs("</span>", fp);
	}
	else
	{
	  write_string(fp, start, OUTPUT_HTML, s - start);
	}

	start = s;
      }

      s          = start + strlen(start) - 1;
      *histate   = HIGHLIGHT_COMMENT1;
      class_name = "comment";
      break;
    }
    else if (*s == '\"' || *s == '\'')
    {
     /*
      * String/character constant...
      */

      if (s > start)
      {
       /*
        * Output current fragment...
        */

	if (class_name)
	{
	  fprintf(fp, "<span class=\"%s\">", class_name);
	  write_string(fp, start, OUTPUT_HTML, s - start);
	  fputs("</span>", fp);
	}
	else
	{
	  write_string(fp, start, OUTPUT_HTML, s - start);
	}

	start = s;
      }

      for (s = start + 1; *s && *s != *start; s ++)
      {
        if (*s == '\\' && s[1])
          s ++;
      }

      if (*s == *start)
        s ++;

      fputs("<span class=\"string\">", fp);
      write_string(fp, start, OUTPUT_HTML, s - start);
      fputs("</span>", fp);

      start = s;
    }
    else
    {
      if (isalnum(*s & 255) || *s == '_' || *s == '.')
      {
       /*
        * Number or keyword...
        */

	if (*histate == HIGHLIGHT_NONE)
	{
	  if (s > start && *histate == HIGHLIGHT_NONE)
	  {
	   /*
	    * End current fragment...
	    */

	    write_string(fp, start, OUTPUT_HTML, s - start);
	    start = s;
	  }

	  if (isdigit(*s & 255) || (*s == '.' && isdigit(s[1] & 255)))
	  {
	    *histate   = HIGHLIGHT_NUMBER;
	    class_name = "number";
	  }
	  else
	  {
	    *histate = HIGHLIGHT_RESERVED;
	  }
	}

        if (*histate == HIGHLIGHT_RESERVED && keyptr < (keyword + sizeof(keyword) - 1))
          *keyptr++ = *s;
      }
      else if (*histate == HIGHLIGHT_NUMBER)
      {
       /*
        * End of number...
        */

	fprintf(fp, "<span class=\"%s\">", class_name);
	write_string(fp, start, OUTPUT_HTML, s - start);
	fputs("</span>", fp);

	start      = s;
	*histate   = HIGHLIGHT_NONE;
	class_name = NULL;
      }
      else if (*histate == HIGHLIGHT_RESERVED)
      {
       /*
        * End of reserved word?
        */

        *keyptr = '\0';
        keyptr  = keyword;

        if (bsearch(&keyptr, words, sizeof(words) / sizeof(words[0]), sizeof(const char *), (int (*)(const void *, const void *))highlight_compare))
        {
         /*
          * Yes, reserved word...
          */

	  fputs("<span class=\"reserved\">", fp);
	  write_string(fp, start, OUTPUT_HTML, s - start);
	  fputs("</span>", fp);
        }
        else
        {
         /*
          * Just some other text...
          */

	  write_string(fp, start, OUTPUT_HTML, s - start);
        }

        start    = s;
        *histate = HIGHLIGHT_NONE;
      }

      s ++;
    }
  }

  if (s > start)
  {
    if (class_name)
    {
      fprintf(fp, "<span class=\"%s\">", class_name);
      write_string(fp, start, OUTPUT_HTML, s - start);
      fputs("</span>", fp);
    }
    else if (*histate == HIGHLIGHT_RESERVED)
    {
      *keyptr = '\0';
      keyptr  = keyword;

      if (bsearch(&keyptr, words, sizeof(words) / sizeof(words[0]), sizeof(const char *), (int (*)(const void *, const void *))highlight_compare))
      {
       /*
	* Yes, reserved word...
	*/

	fputs("<span class=\"reserved\">", fp);
	write_string(fp, start, OUTPUT_HTML, s - start);
	fputs("</span>", fp);
      }
      else
      {
       /*
	* Just some other text...
	*/

	write_string(fp, start, OUTPUT_HTML, s - start);
      }
    }
    else
    {
      write_string(fp, start, OUTPUT_HTML, s - start);
    }
  }

  if (*histate != HIGHLIGHT_COMMENT)
    *histate = HIGHLIGHT_NONE;

  putc('\n', fp);
}


/*
 * 'highlight_compare()' - Compare two reserved words.
 */

static int				/* O - Result of comparison */
highlight_compare(const char **a,	/* I - First word */
                    const char **b)	/* I - Second word */
{
  return (strcmp(*a, *b));
}


/*
 * 'html_gets()' - Get a HTML fragment.
 *
 * Returns a HTML fragment like "<element attr='value'>", "some text", and
 * "</element>".
 */

static char *				/* O - Attribute or `NULL` */
html_gets(FILE	 *fp,			/* I - File to read from */
	  char	 *fragment,		/* I - Fragment string buffer */
	  size_t fragsize)		/* I - Size of buffer */
{
  int	ch;				/* Current character */
  char	*fragptr,			/* Pointer into buffer */
	*fragend;			/* Pointer to end of buffer */


 /*
  * Read the fragment...
  */

  fragptr = fragment;
  fragend = fragment + fragsize - 1;

  if ((ch = getc(fp)) == EOF)
  {
    *fragment = '\0';
    return (NULL);
  }

  *fragptr++ = (char)ch;

  if (ch == '<')
  {
   /*
    * Read element or comment...
    */

    while ((ch = getc(fp)) != EOF)
    {
      if (fragptr < fragend)
	*fragptr++ = (char)ch;

      if (ch == '>')
      {
       /*
        * End of element/comment/processing directive...
        */

	break;
      }
      else if (ch == '\"' || ch == '\'')
      {
       /*
	* Read quoted string...
	*/

	int quote = ch;

	while ((ch = getc(fp)) != EOF)
	{
	  if (fragptr < fragend)
	    *fragptr++ = (char)ch;

	  if (ch == quote)
	    break;
	}
      }
    }

    *fragptr = '\0';
  }
  else
  {
   /*
    * Read text...
    */

    while ((ch = getc(fp)) != EOF)
    {
      if (ch == '<')
      {
	ungetc(ch, fp);
	break;
      }
      else if (fragptr < fragend)
	*fragptr++ = (char)ch;
    }

    *fragptr = '\0';

    html_unescape(fragment);
  }

  return (fragment);
}


/*
 * 'html_unescape()' - Replace &foo; with corresponding characters.
 */

static void
html_unescape(char *buffer)		/* I - Buffer */
{
  char	*inptr,				/* Current input pointer */
	*outptr;			/* Current output pointer */


 /*
  * See if there are any escaped characters to work with...
  */

  if ((inptr = strchr(buffer, '&')) == NULL)
    return;				/* Nope */

  for (outptr = inptr; *inptr;)
  {
    if (*inptr == '&' && strchr(inptr + 1, ';'))
    {
     /*
      * Figure out what kind of escaped character we have...
      */

      inptr ++;
      if (!strncmp(inptr, "amp;", 4))
      {
	inptr += 4;
	*outptr++ = '&';
      }
      else if (!strncmp(inptr, "lt;", 3))
      {
	inptr += 3;
	*outptr++ = '<';
      }
      else if (!strncmp(inptr, "gt;", 3))
      {
	inptr += 3;
	*outptr++ = '>';
      }
      else if (!strncmp(inptr, "quot;", 5))
      {
	inptr += 5;
	*outptr++ = '\"';
      }
      else if (!strncmp(inptr, "apos;", 5))
      {
	inptr += 5;
	*outptr++ = '\'';
      }
      else if (*inptr == '#')
      {
       /*
	* Numeric, copy character over as UTF-8...
	*/

	int ch;				/* Numeric character value */

	inptr ++;
	if (*inptr == 'x')
	  ch = (int)strtol(inptr, NULL, 16);
	else
	  ch = (int)strtol(inptr, NULL, 10);

	if (ch < 0x80)
	{
	 /*
	  * US ASCII
	  */

	  *outptr++ = ch;
	}
	else if (ch < 0x800)
	{
	 /*
	  * Two-byte UTF-8
	  */

	  *outptr++ = 0xc0 | (ch >> 6);
	  *outptr++ = 0x80 | (ch & 0x3f);
	}
	else if (ch < 0x10000)
	{
	 /*
	  * Three-byte UTF-8
	  */

	  *outptr++ = 0xe0 | (ch >> 12);
	  *outptr++ = 0x80 | ((ch >> 6) & 0x3f);
	  *outptr++ = 0x80 | (ch & 0x3f);
	}
	else
	{
	 /*
	  * Four-byte UTF-8
	  */

	  *outptr++ = 0xf0 | (ch >> 18);
	  *outptr++ = 0x80 | ((ch >> 12) & 0x3f);
	  *outptr++ = 0x80 | ((ch >> 6) & 0x3f);
	  *outptr++ = 0x80 | (ch & 0x3f);
	}

	inptr = strchr(inptr, ';') + 1;
      }
      else
      {
       /*
	* Something else not supported by XML...
	*/

	*outptr++ = '&';
      }
    }
    else
    {
     /*
      * Copy literal...
      */

      *outptr++ = *inptr++;
    }
  }

  *outptr = '\0';
}


/*
 * 'is_markdown()' - Determine whether a file is markdown text.
 */

static int				/* O - 1 if markdown, 0 otherwise */
is_markdown(const char *filename)	/* I - File to check */
{
  const char	*ext = filename ? strstr(filename, ".md") : NULL;
					/* Pointer to extension */

  return (ext && !ext[3]);
}


/*
 * 'load_cb()' - Set the type of child nodes.
 */

static mxml_type_t			/* O - Node type */
load_cb(mxml_node_t *node)		/* I - Node */
{
  if (!strcmp(mxmlGetElement(node), "description"))
    return (MXML_OPAQUE);
  else
    return (MXML_TEXT);
}


/*
 * 'markdown_anchor()' - Return the HTML anchor for a given title.
 */

static const char *			/* O - HTML anchor */
markdown_anchor(const char *text)	/* I - Title text */
{
  char          *bufptr;                /* Pointer into buffer */
  static char   buffer[1024];           /* Buffer for anchor string */


  for (bufptr = buffer; *text && bufptr < (buffer + sizeof(buffer) - 1); text ++)
  {
    if ((*text >= '0' && *text <= '9') || (*text >= 'a' && *text <= 'z') || (*text >= 'A' && *text <= 'Z') || *text == '.' || *text == '-')
      *bufptr++ = (char)tolower(*text);
    else if (*text == ' ')
      *bufptr++ = '-';
  }

  *bufptr = '\0';

  return (buffer);
}


/*
 * 'markdown_write_block()' - Write a markdown block.
 */

static void
markdown_write_block(FILE  *out,	/* I - Output file */
                     mmd_t *parent,	/* I - Parent node */
                     int   mode)	/* I - Output mode */
{
  mmd_t		*node;			/* Current child node */
  mmd_type_t	type;			/* Node type */
  int		histate;		/* Highlighting state */


  type = mmdGetType(parent);

  if (mode == OUTPUT_MAN)
  {
    switch (type)
    {
      case MMD_TYPE_BLOCK_QUOTE :
          break;

      case MMD_TYPE_ORDERED_LIST :
          break;

      case MMD_TYPE_UNORDERED_LIST :
          break;

      case MMD_TYPE_LIST_ITEM :
          fputs(".IP \\(bu 5\n", out);
          break;

      case MMD_TYPE_HEADING_1 :
          fputs(".SH ", out);
          break;

      case MMD_TYPE_HEADING_2 :
          fputs(".SS ", out);
          break;

      case MMD_TYPE_HEADING_3 :
      case MMD_TYPE_HEADING_4 :
      case MMD_TYPE_HEADING_5 :
      case MMD_TYPE_HEADING_6 :
      case MMD_TYPE_PARAGRAPH :
          fputs(".PP\n", out);
          break;

      case MMD_TYPE_CODE_BLOCK :
          fputs(".nf\n\n", out);
          for (node = mmdGetFirstChild(parent); node; node = mmdGetNextSibling(node))
          {
            fputs("    ", out);
            write_string(out, mmdGetText(node), mode, 0);
          }
          fputs(".fi\n", out);
          return;

      case MMD_TYPE_METADATA :
          return;

      default :
          break;
    }

    for (node = mmdGetFirstChild(parent); node; node = mmdGetNextSibling(node))
    {
      if (mmdIsBlock(node))
        markdown_write_block(out, node, mode);
      else
        markdown_write_leaf(out, node, mode);
    }

    fputs("\n", out);
  }
  else
  {
    const char	*element,		/* Enclosing element, if any */
		*class_name = NULL;	/* Class name, if any */

    switch (type)
    {
      case MMD_TYPE_BLOCK_QUOTE :
          element = "blockquote";
          break;

      case MMD_TYPE_ORDERED_LIST :
          element = "ol";
          break;

      case MMD_TYPE_UNORDERED_LIST :
          element = "ul";
          break;

      case MMD_TYPE_LIST_ITEM :
          element = "li";
          break;

      case MMD_TYPE_HEADING_1 :
          element    = "h2"; /* Offset since title is H1 for codedoc output */
          class_name = "title";
          break;

      case MMD_TYPE_HEADING_2 :
          element    = "h3"; /* Offset since title is H1 for codedoc output */
          class_name = "title";
          break;

      case MMD_TYPE_HEADING_3 :
          element = "h4"; /* Offset since title is H1 for codedoc output */
          break;

      case MMD_TYPE_HEADING_4 :
          element = "h5"; /* Offset since title is H1 for codedoc output */
          break;

      case MMD_TYPE_HEADING_5 :
          element = "h6"; /* Offset since title is H1 for codedoc output */
          break;

      case MMD_TYPE_HEADING_6 :
          element = "h6";
          break;

      case MMD_TYPE_PARAGRAPH :
          element = "p";
          break;

      case MMD_TYPE_CODE_BLOCK :
          if ((class_name = mmdGetExtra(parent)) != NULL)
	    fprintf(out, "<pre><code class=\"language-%s\">", class_name);
	  else
	    fputs("<pre><code>", out);

          for (node = mmdGetFirstChild(parent), histate = HIGHLIGHT_NONE; node; node = mmdGetNextSibling(node))
          {
            if (class_name && (!strcmp(class_name, "c") || !strcmp(class_name, "cpp")))
              highlight_c_string(out, mmdGetText(node), &histate);
            else
              write_string(out, mmdGetText(node), mode, 0);
	  }
          fputs("</code></pre>\n", out);
          return;

      case MMD_TYPE_THEMATIC_BREAK :
          if (mode == OUTPUT_EPUB)
            fputs("<hr />\n", out);
          else
            fputs("<hr>\n", out);
          return;

      case MMD_TYPE_TABLE :
	  element = "table";
	  break;

      case MMD_TYPE_TABLE_HEADER :
	  element = "thead";
	  break;

      case MMD_TYPE_TABLE_BODY :
	  element = "tbody";
	  break;

      case MMD_TYPE_TABLE_ROW :
	  element = "tr";
	  break;

      case MMD_TYPE_TABLE_HEADER_CELL :
	  element = "th";
	  break;

      case MMD_TYPE_TABLE_BODY_CELL_LEFT :
	  element = "td";
	  break;

      case MMD_TYPE_TABLE_BODY_CELL_CENTER :
	  element    = "td";
	  class_name = "center";
	  break;

      case MMD_TYPE_TABLE_BODY_CELL_RIGHT :
	  element    = "td";
	  class_name = "right";
	  break;

      default :
          element = NULL;
          break;
    }

    if (type >= MMD_TYPE_HEADING_1 && type <= MMD_TYPE_HEADING_6)
    {
     /*
      * Add an anchor...
      */

      if (class_name)
	fprintf(out, "<%s class=\"%s\" id=\"", element, class_name);
      else
	fprintf(out, "<%s id=\"", element);
      for (node = mmdGetFirstChild(parent); node; node = mmdGetNextSibling(node))
      {
        if (mmdGetWhitespace(node))
          fputc('-', out);

        fputs(markdown_anchor(mmdGetText(node)), out);
      }
      fputs("\">", out);
    }
    else if (element)
    {
      if (class_name)
	fprintf(out, "<%s class=\"%s\">", element, class_name);
      else
	fprintf(out, "<%s>%s", element, type <= MMD_TYPE_UNORDERED_LIST ? "\n" : "");
    }

    for (node = mmdGetFirstChild(parent); node; node = mmdGetNextSibling(node))
    {
      if (mmdIsBlock(node))
        markdown_write_block(out, node, mode);
      else
        markdown_write_leaf(out, node, mode);
    }

    if (element)
      fprintf(out, "</%s>\n", element);
  }
}


/*
 * 'markdown_write_leaf()' - Write an leaf markdown node.
 */

static void
markdown_write_leaf(FILE  *out,		/* I - Output file */
                    mmd_t *node,	/* I - Node to write */
                    int   mode)		/* I - Output mode */
{
  mmd_type_t	type,			/* Current leaf node type */
		prev_type,		/* Previous leaf node type */
		next_type;		/* Next leaf node type */
  const char	*text,			/* Text to write */
		*url;			/* URL to write */
  char		temp[1024],		/* Temporary string for text + width */
		*widthspec,		/* Pointer to width specification, if any */
		*heightspec;		/* Pointer to height specification, if any */


  type  = mmdGetType(node);
  text  = mmdGetText(node);
  url   = mmdGetURL(node);

  if (mode == OUTPUT_MAN)
  {
    const char *suffix = NULL;		/* Trailing string */

    switch (type)
    {
      case MMD_TYPE_EMPHASIZED_TEXT :
          if (mmdGetWhitespace(node))
            fputc('\n', out);

          fputs(".I ", out);
          suffix = "\n";
          break;

      case MMD_TYPE_STRONG_TEXT :
          if (mmdGetWhitespace(node))
            fputc('\n', out);

          fputs(".B ", out);
          suffix = "\n";
          break;

      case MMD_TYPE_HARD_BREAK :
          if (mmdGetWhitespace(node))
            fputc('\n', out);

          fputs(".PP\n", out);
          return;

      case MMD_TYPE_SOFT_BREAK :
      case MMD_TYPE_METADATA_TEXT :
          return;

      default :
          if (mmdGetWhitespace(node))
            fputc(' ', out);
          break;
    }

    write_string(out, text, mode, 0);

    if (suffix)
      fputs(suffix, out);
  }
  else
  {
    const char	*element;		/* Encoding element, if any */

    if (mmdGetWhitespace(node))
      fputc(' ', out);

    switch (type)
    {
      case MMD_TYPE_EMPHASIZED_TEXT :
          element = "em";
          break;

      case MMD_TYPE_STRONG_TEXT :
          element = "strong";
          break;

      case MMD_TYPE_STRUCK_TEXT :
          element = "del";
          break;

      case MMD_TYPE_LINKED_TEXT :
          element = NULL;
          break;

      case MMD_TYPE_CODE_TEXT :
          element = "code";
          break;

      case MMD_TYPE_IMAGE :
          fputs("<img src=\"", out);
          if (strncmp(url, "http://", 7) && strncmp(url, "https://", 8))
          {
           /*
            * Local file so strip any directory info...
            */

            const char	*baseurl;	/* Base name of URL */


            if ((baseurl = strrchr(url, '/')) != NULL)
              baseurl ++;
            else
              baseurl = url;

	    write_string(out, baseurl, mode, 0);
	  }
	  else
	  {
	   /*
	    * Remote URL so use as-is...
	    */

	    write_string(out, url, mode, 0);
	  }

          strncpy(temp, text, sizeof(temp) - 1);
          temp[sizeof(temp) - 1] = '\0';

          if ((widthspec = strstr(temp, "::")) != NULL)
          {
            *widthspec = '\0';
            widthspec += 2;

            if ((heightspec = strchr(widthspec, 'x')) != NULL)
	      *heightspec++ = '\0';
	  }
	  else
	  {
	    heightspec = NULL;
          }

	  if (widthspec && *widthspec)
	    fprintf(out, "\" width=\"%s", widthspec);
	  if (heightspec && *heightspec)
	    fprintf(out, "\" height=\"%s", heightspec);

          fputs("\" alt=\"", out);
          write_string(out, text, mode, 0);
          if (mode == OUTPUT_EPUB)
            fputs("\" />", out);
          else
            fputs("\">", out);
          return;

      case MMD_TYPE_HARD_BREAK :
	  if (mmdGetType(mmdGetParent(node)) < MMD_TYPE_HEADING_1 || mmdGetType(mmdGetParent(node)) > MMD_TYPE_HEADING_6)
	  {
	    if (mode == OUTPUT_EPUB)
	      fputs("<br />\n", out);
	    else
	      fputs("<br>\n", out);
	  }
          return;

      case MMD_TYPE_SOFT_BREAK :
          if (mode == OUTPUT_EPUB)
            fputs("<wbr />", out);
          else
            fputs("<wbr>", out);
          return;

      case MMD_TYPE_METADATA_TEXT :
          return;

      default :
          element = NULL;
          break;
    }

    prev_type = mmdGetType(mmdGetPrevSibling(node));
    next_type = mmdGetType(mmdGetNextSibling(node));

    if (url)
    {
      const char *prev_url = mmdGetURL(mmdGetPrevSibling(node));
      const char *title = mmdGetExtra(node);

      if (!prev_url || strcmp(prev_url, url))
      {
	if (!strcmp(url, "@"))
	  fprintf(out, "<a href=\"#%s\"", markdown_anchor(text));
	else if (!strcmp(url, "@@"))
	  fprintf(out, "<a href=\"#%s\"", text);
	else
	  fprintf(out, "<a href=\"%s\"", url);

	if (title)
	{
	  fputs(" title=\"", out);
	  write_string(out, title, mode, 0);
	  fputs("\">", out);
	}
	else
	  putc('>', out);
      }
    }

    if (element && prev_type != type)
      fprintf(out, "<%s>", element);

    write_string(out, text, mode, 0);

    if (element && next_type != type)
      fprintf(out, "</%s>", element);

    if (url)
    {
      const char *next_url = mmdGetURL(mmdGetNextSibling(node));

      if (!next_url || strcmp(next_url, url))
	fputs("</a>", out);
    }
  }
}


/*
 * 'new_documentation()' - Create a new documentation tree.
 */

static mxml_node_t *			/* O - New documentation */
new_documentation(mxml_node_t **codedoc)/* O - codedoc node */
{
  mxml_node_t	*doc;			/* New documentation */


 /*
  * Create an empty XML documentation file...
  */

  doc = mxmlNewXML(NULL);

  *codedoc = mxmlNewElement(doc, "codedoc");

  mxmlElementSetAttr(*codedoc, "xmlns", "https://www.msweet.org");
  mxmlElementSetAttr(*codedoc, "xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
  mxmlElementSetAttr(*codedoc, "xsi:schemaLocation", "https://www.msweet.org/codedoc/codedoc.xsd");

  return (doc);
}


/*
 * 'safe_strcpy()' - Copy a string allowing for overlapping strings.
 */

static void
safe_strcpy(char       *dst,		/* I - Destination string */
            const char *src)		/* I - Source string */
{
  while (*src)
    *dst++ = *src++;

  *dst = '\0';
}


/*
 * 'scan_file()' - Scan a source file.
 */

static int				/* O  - 1 on success, 0 on error */
scan_file(filebuf_t   *file,		/* I  - File to scan */
          mxml_node_t *tree,		/* I  - Function tree */
          const char  *nsname,		/* I  - Namespace name */
          mmd_t       **body)		/* IO - Body markdown text */
{
  int		state,			/* Current parser state */
		braces,			/* Number of braces active */
		parens,			/* Number of active parenthesis */
		tree_is_scu;		/* Is the tree a struct, class, or union? */
  int		ch;			/* Current character */
  stringbuf_t	buffer;			/* String buffer */
  const char	*scope;			/* Current variable/function scope */
  mxml_node_t	*comment,		/* <comment> node */
		*constant,		/* <constant> node */
		*enumeration,		/* <enumeration> node */
		*function,		/* <function> node */
		*fstructclass,		/* function struct/class node */
		*structclass,		/* <struct> or <class> node */
		*typedefnode,		/* <typedef> node */
		*variable,		/* <variable> or <argument> node */
		*returnvalue,		/* <returnvalue> node */
		*type,			/* <type> node */
		*description,		/* <description> node */
		*node,			/* Current node */
		*child,			/* First child node */
		*next;			/* Next node */
  int		whitespace;		/* Current whitespace value */
  const char	*string,		/* Current string value */
		*next_string;		/* Next string value */
  int		nskeyword = 0;		/* Namespace keyword seen? */
  char		nsnamestr[1024] = "";	/* Namespace name string */
#if DEBUG > 1
  mxml_node_t	*temp;			/* Temporary node */
  int		oldstate,		/* Previous state */
		oldch;			/* Old character */
  static const char *states[] =		/* State strings */
		{
		  "STATE_NONE",
		  "STATE_PREPROCESSOR",
		  "STATE_C_COMMENT",
		  "STATE_CXX_COMMENT",
		  "STATE_STRING",
		  "STATE_CHARACTER",
		  "STATE_IDENTIFIER"
		};
#endif /* DEBUG > 1 */


  DEBUG_printf("scan_file(file.filename=\"%s\", .fp=%p, tree=%p, nsname=\"%s\", body=%p)\n", file->filename, file->fp, tree, nsname ? nsname : "(null)", (void *)*body);

 /*
  * Initialize the finite state machine...
  */

  state        = STATE_NONE;
  braces       = 0;
  parens       = 0;
  comment      = mxmlNewElement(MXML_NO_PARENT, "temp");
  constant     = NULL;
  enumeration  = NULL;
  function     = NULL;
  variable     = NULL;
  returnvalue  = NULL;
  type         = NULL;
  description  = NULL;
  typedefnode  = NULL;
  structclass  = NULL;
  fstructclass = NULL;
  tree_is_scu  = !strcmp(mxmlGetElement(tree), "class") || !strcmp(mxmlGetElement(tree), "struct") || !strcmp(mxmlGetElement(tree), "union");

  if (!strcmp(mxmlGetElement(tree), "class"))
    scope = "private";
  else
    scope = NULL;

  stringbuf_clear(&buffer);

 /*
  * Read until end-of-file...
  */

  while ((ch = filebuf_getc(file)) != EOF)
  {
#if DEBUG > 1
    oldstate = state;
    oldch    = ch;
#endif /* DEBUG > 1 */

    switch (state)
    {
      case STATE_NONE :			/* No state - whitespace, etc. */
          switch (ch)
	  {
	    case '/' :			/* Possible C/C++ comment */
	        ch = filebuf_getc(file);

		stringbuf_clear(&buffer);

		if (ch == '*')
		  state = STATE_C_COMMENT;
		else if (ch == '/')
		{
		  state = STATE_CXX_COMMENT;

		  if ((ch = filebuf_getc(file)) != ' ')
		    filebuf_ungetc(file, ch);
		}
		else
		{
		  filebuf_ungetc(file, ch);

		  if (type)
		  {
                    DEBUG_puts("Identifier: <<<< / >>>\n");

                    ch = get_nth_text(type, -1, NULL)[0];
		    mxmlNewText(type, isalnum(ch & 255) || ch == '_', "/");
		  }
		}
		break;

	    case '#' :			/* Preprocessor */
	        DEBUG_puts("    #preprocessor...\n");

	        state = STATE_PREPROCESSOR;
	        while (mxmlGetFirstChild(comment))
	          mxmlDelete(mxmlGetFirstChild(comment));
		break;

            case '\'' :			/* Character constant */
	        state = STATE_CHARACTER;

		stringbuf_clear(&buffer);
		stringbuf_append(&buffer, ch);
		break;

            case '\"' :			/* String constant */
	        state = STATE_STRING;

		stringbuf_clear(&buffer);
		stringbuf_append(&buffer, ch);
		break;

            case '{' :
                if (nskeyword)
                {
                  if (!scan_file(file, tree, nsnamestr, body))
		  {
		    mxmlDelete(comment);
		    return (0);
		  }

		  nskeyword    = 0;
		  nsnamestr[0] = '\0';
                  break;
                }

                string      = get_nth_text(type, 0, &whitespace);
                next_string = get_nth_text(type, 1, NULL);

#ifdef DEBUG
	        DEBUG_printf("    open brace, function=%p, type=%p...\n", function, type);
                if (type)
                  DEBUG_printf("    type->child=\"%s\"...\n", string);
#endif /* DEBUG */

	        if (function)
		{
                  mxml_node_t *temptype = mxmlFindElement(returnvalue, returnvalue, "type", NULL, NULL, MXML_DESCEND);

		  DEBUG_printf("    returnvalue type=%p(%s)\n", temptype, temptype ? mxmlGetText(mxmlGetFirstChild(temptype), NULL) : "null");

		  if (temptype && mxmlGetFirstChild(temptype) && !strcmp(mxmlGetText(mxmlGetFirstChild(temptype), NULL), "static") && !strcmp(mxmlGetElement(tree), "codedoc"))
                  {
                   /*
                    * Remove static functions...
                    */

                    DEBUG_puts("    DELETING STATIC FUNCTION\n");

                    mxmlDelete(function);
                  }
                  else if (fstructclass)
		  {
		    sort_node(fstructclass, function);
		    fstructclass = NULL;
		  }
		  else
		    sort_node(tree, function);

		  function    = NULL;
		  returnvalue = NULL;
		}
		else if (type && string && ((!strcmp(string, "typedef") && next_string && (!strcmp(next_string, "struct") || !strcmp(next_string, "union") || !strcmp(next_string, "class"))) || !strcmp(string, "union") || !strcmp(string, "struct") || !strcmp(string, "class")))
		{
		 /*
		  * Start of a class or structure...
		  */

		  if (!strcmp(string, "typedef"))
		  {
                    DEBUG_puts("    starting typedef...\n");

		    typedefnode = mxmlNewElement(MXML_NO_PARENT, "typedef");
		    mxmlDelete(mxmlGetFirstChild(type));

		    string      = next_string;
		    next_string = get_nth_text(type, 1, NULL);
		  }
		  else
		    typedefnode = NULL;

		  structclass = mxmlNewElement(MXML_NO_PARENT, string);

#ifdef DEBUG
                  DEBUG_printf("%c%s: <<<< %s >>>\n", toupper(string[0]), string + 1, next_string ? next_string : "(noname)");

                  DEBUG_puts("    type =");
                  for (node = mxmlGetFirstChild(type); node; node = mxmlGetNextSibling(node))
		    DEBUG_printf(" \"%s\"", mxmlGetText(node, NULL));
		  DEBUG_puts("\n");

                  DEBUG_printf("    scope = %s\n", scope ? scope : "(null)");
#endif /* DEBUG */

                  if (next_string)
		  {
		    if (nsname)
		    {
		      mxmlElementSetAttrf(structclass, "name", "%s::%s", nsname, next_string);
		      mxmlSetText(get_nth_child(type, 1), 1, mxmlElementGetAttr(structclass, "name"));
		    }
		    else
		      mxmlElementSetAttr(structclass, "name", next_string);
		    sort_node(tree, structclass);
		  }

                  child = mxmlGetFirstChild(type);

                  if (typedefnode && child)
                  {
                    clear_whitespace(mxmlGetFirstChild(type));
		  }
                  else if (structclass && child && mxmlGetNextSibling(child) && mxmlGetNextSibling(mxmlGetNextSibling(child)))
		  {
		    char temp[65536], *tempptr;

		    for (tempptr = temp, node = mxmlGetNextSibling(mxmlGetNextSibling(child)); node; tempptr += strlen(temp))
		    {
		      string = mxmlGetText(node, &whitespace);

		      if (whitespace && tempptr > temp && tempptr < (temp + sizeof(temp) - 1))
			*tempptr++ = ' ';

		      strlcpy(tempptr, string, sizeof(temp) - (size_t)(tempptr - temp));

		      next = mxmlGetNextSibling(node);
		      mxmlDelete(node);
		      node = next;
		    }

		    mxmlElementSetAttr(structclass, "parent", temp);

		    mxmlDelete(type);
		    type = NULL;
		  }
		  else
		  {
		    mxmlDelete(type);
		    type = NULL;
		  }

		  if (typedefnode && mxmlGetLastChild(comment))
		  {
		   /*
		    * Copy comment for typedef as well as class/struct/union...
		    */

		    mxmlNewOpaque(comment, mxmlGetOpaque(mxmlGetLastChild(comment)));
		    description = mxmlNewElement(typedefnode, "description");

		    DEBUG_printf("    duplicating comment %p/%p for typedef...\n", mxmlGetLastChild(comment), mxmlGetFirstChild(comment));

		    update_comment(typedefnode, mxmlGetLastChild(comment));
		    mxmlAdd(description, MXML_ADD_AFTER, MXML_ADD_TO_PARENT, mxmlGetLastChild(comment));
		  }

		  description = mxmlNewElement(structclass, "description");

		  DEBUG_printf("    adding comment %p/%p to %s...\n", mxmlGetLastChild(comment), mxmlGetFirstChild(comment), mxmlGetElement(structclass));

		  update_comment(structclass, mxmlGetLastChild(comment));
		  mxmlAdd(description, MXML_ADD_AFTER, MXML_ADD_TO_PARENT, mxmlGetLastChild(comment));

                  if (!scan_file(file, structclass, nsname, body))
		  {
		    mxmlDelete(comment);
		    return (0);
		  }

                  DEBUG_puts("    ended typedef...\n");

                  structclass = NULL;
                  break;
                }
		else if (type && string && next_string && (!strcmp(string, "enum") || (!strcmp(string, "typedef") && !strcmp(next_string, "enum"))))
                {
		 /*
		  * Enumeration type...
		  */

		  if (!strcmp(string, "typedef"))
		  {
                    DEBUG_puts("    starting typedef...\n");

		    typedefnode = mxmlNewElement(MXML_NO_PARENT, "typedef");
		    mxmlDelete(mxmlGetFirstChild(type));
		    string      = next_string;
		    next_string = get_nth_text(type, 1, NULL);
		  }
		  else
		    typedefnode = NULL;

		  enumeration = mxmlNewElement(MXML_NO_PARENT, "enumeration");

                  DEBUG_printf("Enumeration: <<<< %s >>>\n", next_string ? next_string : "(noname)");

                  if (next_string)
		  {
		    if (nsname)
		    {
		      mxmlElementSetAttrf(enumeration, "name", "%s::%s", nsname, next_string);
		      mxmlSetText(get_nth_child(type, 1), 1, mxmlElementGetAttr(enumeration, "name"));
		    }
		    else
		      mxmlElementSetAttr(enumeration, "name", next_string);

		    sort_node(tree, enumeration);
		  }

                  if (typedefnode && mxmlGetFirstChild(type))
                  {
                    clear_whitespace(mxmlGetFirstChild(type));
		  }
                  else
		  {
		    mxmlDelete(type);
		    type = NULL;
		  }

		  if (typedefnode && mxmlGetLastChild(comment))
		  {
		   /*
		    * Copy comment for typedef as well as class/struct/union...
		    */

		    mxmlNewOpaque(comment, mxmlGetOpaque(mxmlGetLastChild(comment)));
		    description = mxmlNewElement(typedefnode, "description");

		    DEBUG_printf("    duplicating comment %p/%p for typedef...\n", mxmlGetLastChild(comment), mxmlGetFirstChild(comment));

		    update_comment(typedefnode, mxmlGetLastChild(comment));
		    mxmlAdd(description, MXML_ADD_AFTER, MXML_ADD_TO_PARENT, mxmlGetLastChild(comment));
		  }

		  description = mxmlNewElement(enumeration, "description");

		  DEBUG_printf("    adding comment %p/%p to enumeration...\n", mxmlGetLastChild(comment), mxmlGetFirstChild(comment));

		  update_comment(enumeration, mxmlGetLastChild(comment));
		  mxmlAdd(description, MXML_ADD_AFTER, MXML_ADD_TO_PARENT, mxmlGetLastChild(comment));
		}
		else if (type && string && !strcmp(string, "extern"))
                {
                  if (!scan_file(file, tree, nsname, body))
		  {
		    mxmlDelete(comment);
		    return (0);
		  }
                }
		else if (type)
		{
		  mxmlDelete(type);
		  type = NULL;
		}

	        braces ++;
		function = NULL;
		variable = NULL;
		break;

            case '}' :
	        DEBUG_puts("    close brace...\n");

                if (structclass)
		  scope = NULL;

                if (!typedefnode)
		  enumeration = NULL;

		constant    = NULL;
		structclass = NULL;

	        if (braces > 0)
	        {
		  braces --;
		  if (braces == 0)
		  {
		    while (mxmlGetFirstChild(comment))
		      mxmlDelete(mxmlGetFirstChild(comment));
		  }
		}
		else
		{
		  mxmlDelete(comment);
		  return (1);
		}
		break;

            case '(' :
		if (type)
		{
                  DEBUG_puts("Identifier: <<<< ( >>>\n");

		  mxmlNewText(type, 0, "(");
		}

	        parens ++;
		break;

            case ')' :
		if (type && parens)
		{
                  DEBUG_puts("Identifier: <<<< ) >>>\n");

		  mxmlNewText(type, 0, ")");
		}

                if (function && type && !parens)
		{
		 /*
		  * Check for "void" argument...
		  */

		  if ((child = mxmlGetFirstChild(type)) != NULL && mxmlGetNextSibling(child))
		    variable = add_variable(function, "argument", type);
		  else
		    mxmlDelete(type);

		  type = NULL;
		}

	        if (parens > 0)
		  parens --;
		break;

	    case ';' :
                DEBUG_puts("Identifier: <<<< ; >>>\n");
		DEBUG_printf("    enumeration=%p, function=%p, type=%p, type->child=%p, typedefnode=%p\n", enumeration, function, type, mxmlGetFirstChild(type), typedefnode);

		if (function)
		{
                  mxml_node_t *temptype = mxmlFindElement(returnvalue, returnvalue, "type", NULL, NULL, MXML_DESCEND);

                  DEBUG_printf("    returnvalue type=%p(%s)\n", temptype, temptype ? mxmlGetText(mxmlGetFirstChild(temptype), NULL) : "null");

		  if (temptype && mxmlGetFirstChild(temptype) && !strcmp(get_nth_text(temptype, 0, NULL), "static") && !strcmp(mxmlGetElement(tree), "codedoc"))
                  {
                   /*
                    * Remove static functions...
                    */

                    DEBUG_puts("    DELETING STATIC FUNCTION\n");

                    mxmlDelete(function);
                  }
                  else if (!strcmp(mxmlGetElement(tree), "class"))
		  {
		    DEBUG_puts("    ADDING FUNCTION TO CLASS\n");
		    sort_node(tree, function);
		  }
		  else
		    mxmlDelete(function);

		  function    = NULL;
		  variable    = NULL;
		  returnvalue = NULL;
		}

		if (type)
		{
		 /*
		  * See if we have a typedef...
		  */

                  string = get_nth_text(type, 0, NULL);
		  if (string && !strcmp(string, "typedef"))
		  {
		   /*
		    * Yes, add it!
		    */

		    typedefnode = mxmlNewElement(MXML_NO_PARENT, "typedef");

		    for (node = get_nth_child(type, 1); node; node = mxmlGetNextSibling(node))
		      if (!strcmp(mxmlGetText(node, NULL), "("))
			break;

                    if (node)
		    {
		      for (node = mxmlGetNextSibling(node); node; node = mxmlGetNextSibling(node))
			if (strcmp(mxmlGetText(node, NULL), "*"))
			  break;
                    }

                    if (!node)
		      node = mxmlGetLastChild(type);

		    DEBUG_printf("    ADDING TYPEDEF FOR %p(%s)...\n", node, mxmlGetText(node, NULL));

		    mxmlElementSetAttr(typedefnode, "name", mxmlGetText(node, NULL));
		    sort_node(tree, typedefnode);

                    if (mxmlGetFirstChild(type) != node)
		      mxmlDelete(mxmlGetFirstChild(type));

		    mxmlDelete(node);

		    if (mxmlGetFirstChild(type))
		      clear_whitespace(mxmlGetFirstChild(type));

		    mxmlAdd(typedefnode, MXML_ADD_AFTER, MXML_ADD_TO_PARENT, type);
		    type = NULL;
		    break;
		  }
		  else if (typedefnode && enumeration)
		  {
		   /*
		    * Add enum typedef...
		    */

                    node = mxmlGetFirstChild(type);

		    DEBUG_printf("    ADDING TYPEDEF FOR %p(%s)...\n", node, mxmlGetText(node, NULL));

		    mxmlElementSetAttr(typedefnode, "name", mxmlGetText(node, NULL));
		    sort_node(tree, typedefnode);
		    mxmlDelete(type);

		    type = mxmlNewElement(typedefnode, "type");
                    mxmlNewText(type, 0, "enum");
		    mxmlNewText(type, 1, mxmlElementGetAttr(enumeration, "name"));
		    enumeration = NULL;
		    type = NULL;
		    break;
		  }

		  mxmlDelete(type);
		  type = NULL;
		}
		break;

	    case ':' :
		if (type)
		{
                  DEBUG_puts("Identifier: <<<< : >>>\n");

		  mxmlNewText(type, 1, ":");
		}
		break;

	    case '*' :
		if (type)
		{
                  DEBUG_puts("Identifier: <<<< * >>>\n");

                  ch = get_nth_text(type, -1, NULL)[0];
		  mxmlNewText(type, isalnum(ch & 255) || ch == '_', "*");
		}
		break;

	    case ',' :
		if (type && !enumeration)
		{
                  DEBUG_puts("Identifier: <<<< , >>>\n");

		  mxmlNewText(type, 0, ",");
		}
		break;

	    case '&' :
		if (type)
		{
                  DEBUG_puts("Identifier: <<<< & >>>\n");

		  mxmlNewText(type, 1, "&");
		}
		break;

	    case '+' :
		if (type)
		{
                  DEBUG_puts("Identifier: <<<< + >>>\n");

                  ch = get_nth_text(type, -1, NULL)[0];
		  mxmlNewText(type, isalnum(ch & 255) || ch == '_', "+");
		}
		break;

	    case '-' :
		if (type)
		{
                  DEBUG_puts("Identifier: <<<< - >>>\n");

                  ch = get_nth_text(type, -1, NULL)[0];
		  mxmlNewText(type, isalnum(ch & 255) || ch == '_', "-");
		}
		break;

	    case '=' :
		if (type)
		{
                  DEBUG_puts("Identifier: <<<< = >>>\n");

                  ch = get_nth_text(type, -1, NULL)[0];
		  mxmlNewText(type, isalnum(ch & 255) || ch == '_', "=");
		}
		break;

            default :			/* Other */
	        if (isalnum(ch & 255) || ch == '_' || ch == '.' || ch == ':' || ch == '~')
		{
		  state = STATE_IDENTIFIER;

		  stringbuf_clear(&buffer);
		  stringbuf_append(&buffer, ch);
		}
		break;
          }
          break;

      case STATE_PREPROCESSOR :		/* Preprocessor directive */
          if (ch == '\n')
	    state = STATE_NONE;
	  else if (ch == '\\')
	    filebuf_getc(file);
          break;

      case STATE_C_COMMENT :		/* Inside a C comment */
          switch (ch)
	  {
	    case '\n' :
	        while ((ch = filebuf_getc(file)) != EOF)
	        {
		  if (ch == '*')
		  {
		    ch = filebuf_getc(file);

		    if (ch == '/')
		    {
		      char *commstr = stringbuf_get(&buffer);

        	      if (mxmlGetFirstChild(child) != mxmlGetLastChild(comment))
		      {
			DEBUG_printf("    removing comment %p(%20.20s), last comment %p(%20.20s)...\n", mxmlGetFirstChild(comment), mxmlGetFirstChild(comment) ? get_nth_text(comment, 0, NULL) : "", mxmlGetLastChild(comment), mxmlGetLastChild(comment) ? get_nth_text(comment, -1, NULL) : "");

			mxmlDelete(mxmlGetFirstChild(comment));

			DEBUG_printf("    new comment %p, last comment %p...\n", mxmlGetFirstChild(comment), mxmlGetLastChild(comment));
		      }

                      DEBUG_printf("    processing comment, variable=%p, constant=%p, typedefnode=%p, tree=\"%s\"\n", variable, constant, typedefnode, mxmlGetElement(tree));

		      if (variable)
		      {
		        if (strstr(commstr, "@private@"))
			{
			 /*
			  * Delete private variables...
			  */

			  mxmlDelete(variable);
			}
			else
			{
			  description = mxmlNewElement(variable, "description");

			  DEBUG_printf("    adding comment %p/%p to variable...\n", mxmlGetLastChild(comment), mxmlGetFirstChild(comment));

			  mxmlNewOpaque(comment, commstr);
			  update_comment(variable, mxmlNewOpaque(description, commstr));
                        }

			variable = NULL;
		      }
		      else if (constant)
		      {
		        if (strstr(commstr, "@private@"))
			{
			 /*
			  * Delete private constants...
			  */

			  mxmlDelete(constant);
			}
			else
			{
			  description = mxmlNewElement(constant, "description");

			  DEBUG_printf("    adding comment %p/%p to constant...\n", mxmlGetLastChild(comment), mxmlGetFirstChild(comment));

			  mxmlNewOpaque(comment, commstr);
			  update_comment(constant, mxmlNewOpaque(description, commstr));
			}

			constant = NULL;
		      }
		      else if (typedefnode)
		      {
		        if (strstr(commstr, "@private@"))
			{
			 /*
			  * Delete private typedefs...
			  */

			  mxmlDelete(typedefnode);

			  if (structclass)
			  {
			    mxmlDelete(structclass);
			    structclass = NULL;
			  }

			  if (enumeration)
			  {
			    mxmlDelete(enumeration);
			    enumeration = NULL;
			  }
			}
			else
			{
			  description = mxmlNewElement(typedefnode, "description");

			  DEBUG_printf("    adding comment %p/%p to typedef %s...\n", mxmlGetLastChild(comment), mxmlGetFirstChild(comment), mxmlElementGetAttr(typedefnode, "name"));

			  mxmlNewOpaque(comment, commstr);
			  update_comment(typedefnode, mxmlNewOpaque(description, commstr));

			  if (structclass)
			  {
			    description = mxmlNewElement(structclass, "description");
			    update_comment(structclass, mxmlNewOpaque(description, commstr));
			  }
			  else if (enumeration)
			  {
			    description = mxmlNewElement(enumeration, "description");
			    update_comment(enumeration, mxmlNewOpaque(description, commstr));
			  }
			}

			typedefnode = NULL;
		      }
		      else if (strcmp(mxmlGetElement(tree), "codedoc") && !mxmlFindElement(tree, tree, "description", NULL, NULL, MXML_DESCEND_FIRST))
                      {
        		description = mxmlNewElement(tree, "description");

			DEBUG_printf("    adding comment %p/%p to parent...\n", mxmlGetLastChild(comment), mxmlGetFirstChild(comment));

        		mxmlNewOpaque(comment, commstr);
			update_comment(tree, mxmlNewOpaque(description, commstr));
		      }
		      else if (!strncmp(commstr, "@body@", 6))
		      {
		       /*
		        * Append comment as body text...
		        */

		        *body = mmdLoadString(*body, commstr + 6);
		      }
		      else
		      {
		        DEBUG_printf("    before adding comment, child=%p, last_child=%p\n", mxmlGetFirstChild(comment), mxmlGetLastChild(comment));

        		mxmlNewOpaque(comment, commstr);

		        DEBUG_printf("    after adding comment, child=%p, last_child=%p\n", mxmlGetFirstChild(comment), mxmlGetLastChild(comment));
                      }

		      DEBUG_printf("C comment: <<<< %s >>>\n", commstr);

		      state = STATE_NONE;
		      break;
		    }
		    else
		    {
		      while (ch == '*')
		        ch = filebuf_getc(file);

		      if (ch != ' ')
		        filebuf_ungetc(file, ch);

		      ch = EOF;
		      break;
		    }
		  }
		  else if (ch == '\n' && stringbuf_length(&buffer) > 0)
		    stringbuf_append(&buffer, ch);
		  else if (!isspace(ch & 255))
		    break;
		}

		if (ch != EOF)
		  filebuf_ungetc(file, ch);

                if (stringbuf_length(&buffer) > 0)
		  stringbuf_append(&buffer, '\n');
		break;

	    case '/' :
	        if (ch == '/' && stringbuf_getlast(&buffer) == '*')
		{
		  char *commstr = stringbuf_get(&buffer);
		  char *commptr = commstr + strlen(commstr) - 1;

		  while (commptr > commstr && (commptr[-1] == '*' || isspace(commptr[-1] & 255)))
		    commptr --;

		  *commptr = '\0';

        	  if (mxmlGetFirstChild(comment) != mxmlGetLastChild(comment))
		  {
		    DEBUG_printf("    removing comment %p(%20.20s), last comment %p(%20.20s)...\n", mxmlGetFirstChild(comment), mxmlGetFirstChild(comment) ? get_nth_text(comment, 0, NULL) : "", mxmlGetLastChild(comment), mxmlGetLastChild(comment) ? get_nth_text(comment, -1, NULL) : "");

		    mxmlDelete(mxmlGetFirstChild(comment));

		    DEBUG_printf("    new comment %p, last comment %p...\n", mxmlGetFirstChild(comment), mxmlGetLastChild(comment));
		  }

                  DEBUG_printf("    processing comment, variable=%p, constant=%p, typedefnode=%p, tree=\"%s\"\n", variable, constant, typedefnode, mxmlGetElement(tree));

		  if (variable)
		  {
		    if (strstr(commstr, "@private@"))
		    {
		     /*
		      * Delete private variables...
		      */

		      mxmlDelete(variable);
		    }
		    else
		    {
		      description = mxmlNewElement(variable, "description");

		      DEBUG_printf("    adding comment %p/%p to variable...\n", mxmlGetLastChild(comment), mxmlGetFirstChild(comment));

		      mxmlNewOpaque(comment, commstr);
		      update_comment(variable, mxmlNewOpaque(description, commstr));
                    }

		    variable = NULL;
		  }
		  else if (constant)
		  {
		    if (strstr(commstr, "@private@"))
		    {
		     /*
		      * Delete private constants...
		      */

		      mxmlDelete(constant);
		    }
		    else
		    {
		      description = mxmlNewElement(constant, "description");

		      DEBUG_printf("    adding comment %p/%p to constant...\n", mxmlGetLastChild(comment), mxmlGetFirstChild(comment));

		      mxmlNewOpaque(comment, commstr);
		      update_comment(constant, mxmlNewOpaque(description, commstr));
		    }

		    constant = NULL;
		  }
		  else if (typedefnode)
		  {
		    if (strstr(commstr, "@private@"))
		    {
		     /*
		      * Delete private typedefs...
		      */

		      mxmlDelete(typedefnode);

		      if (structclass)
		      {
			mxmlDelete(structclass);
			structclass = NULL;
		      }

		      if (enumeration)
		      {
			mxmlDelete(enumeration);
			enumeration = NULL;
		      }
		    }
		    else
		    {
		      description = mxmlNewElement(typedefnode, "description");

		      DEBUG_printf("    adding comment %p/%p to typedef %s...\n", mxmlGetLastChild(comment), mxmlGetFirstChild(comment), mxmlElementGetAttr(typedefnode, "name"));

		      mxmlNewOpaque(comment, commstr);
		      update_comment(typedefnode, mxmlNewOpaque(description, commstr));

		      if (structclass)
		      {
			description = mxmlNewElement(structclass, "description");
			update_comment(structclass, mxmlNewOpaque(description, commstr));
		      }
		      else if (enumeration)
		      {
			description = mxmlNewElement(enumeration, "description");
			update_comment(enumeration, mxmlNewOpaque(description, commstr));
		      }
		    }

		    typedefnode = NULL;
		  }
		  else if (strcmp(mxmlGetElement(tree), "codedoc") && !mxmlFindElement(tree, tree, "description", NULL, NULL, MXML_DESCEND_FIRST))
                  {
        	    description = mxmlNewElement(tree, "description");

		    DEBUG_printf("    adding comment %p/%p to parent...\n", mxmlGetLastChild(comment), mxmlGetFirstChild(comment));

		    mxmlNewOpaque(comment, commstr);
		    update_comment(tree, mxmlNewOpaque(description, commstr));
		  }
		  else
        	    mxmlNewOpaque(comment, commstr);

		  DEBUG_printf("C comment: <<<< %s >>>\n", commstr);

		  state = STATE_NONE;
		  break;
		}

	    default :
	        if (ch == ' ' && stringbuf_length(&buffer) == 0)
		  break;

		stringbuf_append(&buffer, ch);
		break;
          }
          break;

      case STATE_CXX_COMMENT :		/* Inside a C++ comment */
          if (ch == '\n')
	  {
	    if ((ch = filebuf_getc(file)) == '/')
	    {
	      if ((ch = filebuf_getc(file)) == '/')
	      {
		stringbuf_append(&buffer, '\n');

                if ((ch = filebuf_getc(file)) != ' ')
                  filebuf_ungetc(file, ch);
		break;
	      }
	      else
	      {
		filebuf_ungetc(file, ('/' << 8) | ch);
	      }
	    }
	    else
	      filebuf_ungetc(file, ch);

	    char *commstr = stringbuf_get(&buffer);

	    state = STATE_NONE;

            if (mxmlGetFirstChild(comment) != mxmlGetLastChild(comment))
	    {
	      DEBUG_printf("    removing comment %p(%20.20s), last comment %p(%20.20s)...\n", mxmlGetFirstChild(comment), mxmlGetFirstChild(comment) ? get_nth_text(comment, 0, NULL) : "", mxmlGetLastChild(comment), mxmlGetLastChild(comment) ? get_nth_text(comment, -1, NULL) : "");

	      mxmlDelete(mxmlGetFirstChild(comment));

	      DEBUG_printf("    new comment %p, last comment %p...\n", mxmlGetFirstChild(comment), mxmlGetLastChild(comment));
	    }

	    if (variable)
	    {
	      if (strstr(commstr, "@private@"))
	      {
	       /*
		* Delete private variables...
		*/

		mxmlDelete(variable);
	      }
	      else
	      {
		description = mxmlNewElement(variable, "description");

		DEBUG_printf("    adding comment %p/%p to variable...\n", mxmlGetLastChild(comment), mxmlGetFirstChild(comment));

		mxmlNewOpaque(comment, commstr);
		update_comment(variable, mxmlNewOpaque(description, commstr));
              }

	      variable = NULL;
	    }
	    else if (constant)
	    {
	      if (strstr(commstr, "@private@"))
	      {
	       /*
		* Delete private constants...
		*/

		mxmlDelete(constant);
	      }
	      else
	      {
		description = mxmlNewElement(constant, "description");

		DEBUG_printf("    adding comment %p/%p to constant...\n", mxmlGetLastChild(comment), mxmlGetFirstChild(comment));

		mxmlNewOpaque(comment, commstr);
		update_comment(constant, mxmlNewOpaque(description, commstr));
              }

	      constant = NULL;
	    }
	    else if (typedefnode)
	    {
	      if (strstr(commstr, "@private@"))
	      {
	       /*
		* Delete private typedefs...
		*/

		mxmlDelete(typedefnode);

		if (structclass)
		{
		  mxmlDelete(structclass);
		  structclass = NULL;
		}

		if (enumeration)
		{
		  mxmlDelete(enumeration);
		  enumeration = NULL;
		}
	      }
	      else
	      {
		description = mxmlNewElement(typedefnode, "description");

		DEBUG_printf("    adding comment %p/%p to typedef %s...\n", mxmlGetLastChild(comment), mxmlGetFirstChild(comment), mxmlElementGetAttr(typedefnode, "name"));

		mxmlNewOpaque(comment, commstr);
		update_comment(typedefnode, mxmlNewOpaque(description, commstr));

		if (structclass)
		{
		  description = mxmlNewElement(structclass, "description");
		  update_comment(structclass, mxmlNewOpaque(description, commstr));
		}
		else if (enumeration)
		{
		  description = mxmlNewElement(enumeration, "description");
		  update_comment(enumeration, mxmlNewOpaque(description, commstr));
		}
              }

	      typedefnode = NULL;
	    }
	    else if (strcmp(mxmlGetElement(tree), "codedoc") && !mxmlFindElement(tree, tree, "description", NULL, NULL, MXML_DESCEND_FIRST))
            {
              description = mxmlNewElement(tree, "description");

	      DEBUG_printf("    adding comment %p/%p to parent...\n", mxmlGetLastChild(comment), mxmlGetFirstChild(comment));

	      mxmlNewOpaque(comment, commstr);
	      update_comment(tree, mxmlNewOpaque(description, commstr));
	    }
	    else if (!strncmp(commstr, "@body@", 6))
	    {
	     /*
	      * Append comment as body text...
	      */

	      *body = mmdLoadString(*body, commstr + 6);
	    }
	    else
              mxmlNewOpaque(comment, commstr);

	    DEBUG_printf("C++ comment: <<<< %s >>>\n", commstr);
	  }
	  else
	    stringbuf_append(&buffer, ch);
          break;

      case STATE_STRING :		/* Inside a string constant */
	  stringbuf_append(&buffer, ch);

          if (ch == '\\')
	    stringbuf_append(&buffer, filebuf_getc(file));
	  else if (ch == '\"')
	  {
	    if (type)
	      mxmlNewText(type, mxmlGetFirstChild(type) != NULL, stringbuf_get(&buffer));

	    state = STATE_NONE;
	  }
          break;

      case STATE_CHARACTER :		/* Inside a character constant */
	  stringbuf_append(&buffer, ch);

          if (ch == '\\')
	    stringbuf_append(&buffer, filebuf_getc(file));
	  else if (ch == '\'')
	  {
	    if (type)
	      mxmlNewText(type, mxmlGetFirstChild(type) != NULL, stringbuf_get(&buffer));

	    state = STATE_NONE;
	  }
          break;

      case STATE_IDENTIFIER :		/* Inside a keyword or identifier */
	  if (isalnum(ch & 255) || ch == '_' || ch == '[' || ch == ']' || (ch == ',' && (parens > 1 || (type && !enumeration && !function))) || (ch == ',' && parens > 0 && typedefnode) || ch == ':' || ch == '.' || ch == '~')
	  {
	    stringbuf_append(&buffer, ch);
	  }
	  else
	  {
	    char *ptr, *str = stringbuf_get(&buffer);

	    filebuf_ungetc(file, ch);

	    state = STATE_NONE;

            DEBUG_printf("    braces=%d, type=%p, type->child=%p, buffer=\"%s\"\n", braces, type, type ? mxmlGetFirstChild(type) : NULL, str);

            if (!braces)
	    {
	      if (!strcmp(str, "namespace"))
	      {
	        nskeyword = 1;
	        break;
	      }
	      else if (nskeyword)
	      {
	        strlcpy(nsnamestr, str, sizeof(nsnamestr));
	        break;
	      }

	      if (!type || !mxmlGetFirstChild(type))
	      {
		if (!strcmp(mxmlGetElement(tree), "class"))
		{
		  if (!strcmp(str, "public") || !strcmp(str, "public:"))
		  {
		    scope = "public";

		    DEBUG_puts("    scope = public\n");
		    break;
		  }
		  else if (!strcmp(str, "private") || !strcmp(str, "private:"))
		  {
		    scope = "private";

		    DEBUG_puts("    scope = private\n");
		    break;
		  }
		  else if (!strcmp(str, "protected") || !strcmp(str, "protected:"))
		  {
		    scope = "protected";

		    DEBUG_puts("    scope = protected\n");
		    break;
		  }
		}
	      }

	      if (!type)
                type = mxmlNewElement(MXML_NO_PARENT, "type");

              DEBUG_printf("    function=%p (%s), type->child=%p, ch='%c', parens=%d\n", function, function ? mxmlElementGetAttr(function, "name") : "null", mxmlGetFirstChild(type), ch, parens);

              if (!function && ch == '(')
	      {
	        if (mxmlGetFirstChild(type) && !strcmp(mxmlGetText(mxmlGetFirstChild(type), NULL), "extern"))
		{
		 /*
		  * Remove external declarations...
		  */

		  mxmlDelete(type);
		  type = NULL;
		  break;
		}

	        function = mxmlNewElement(MXML_NO_PARENT, "function");
		if ((ptr = strchr(str, ':')) != NULL && ptr[1] == ':')
		{
		  char name[1024];	/* Struct/class name */

		  *ptr = '\0';
		  ptr += 2;

                  if (nsname)
                    snprintf(name, sizeof(name), "%s::%s", nsname, str);
                  else
                    strlcpy(name, str, sizeof(name));

                  DEBUG_printf("looking for struct or class '%s' under %p(%s)...\n", name, tree, mxmlGetElement(tree));
		  if ((fstructclass = mxmlFindElement(tree, tree, "class", "name", name, MXML_DESCEND_FIRST)) == NULL)
		    fstructclass = mxmlFindElement(tree, tree, "struct", "name", name, MXML_DESCEND_FIRST);
                  DEBUG_printf("fstructclass=%p\n", fstructclass);
		}
		else
		  ptr = str;

                if (nsname && !fstructclass && !tree_is_scu)
                  mxmlElementSetAttrf(function, "name", "%s::%s", nsname, ptr);
                else
		  mxmlElementSetAttr(function, "name", ptr);

		if (scope)
		  mxmlElementSetAttr(function, "scope", scope);

                DEBUG_printf("function: %s\n", mxmlElementGetAttr(function, "name"));
		DEBUG_printf("    scope = %s\n", scope ? scope : "(null)");
		DEBUG_printf("    comment = %p\n", comment);
		DEBUG_printf("    child = (%p) %s\n", mxmlGetFirstChild(comment), get_nth_text(comment, 0, NULL));
		DEBUG_printf("    last_child = (%p) %s\n", mxmlGetLastChild(comment), get_nth_text(comment, -1, NULL));

                if (mxmlGetLastChild(type) && (strcmp(get_nth_text(type, -1, NULL), "void") || !strcmp(get_nth_text(type, 0, NULL), "static")))
		{
                  returnvalue = mxmlNewElement(function, "returnvalue");

		  mxmlAdd(returnvalue, MXML_ADD_AFTER, MXML_ADD_TO_PARENT, type);

		  description = mxmlNewElement(returnvalue, "description");

		  DEBUG_printf("    adding comment %p/%p to returnvalue...\n", mxmlGetLastChild(comment), mxmlGetFirstChild(comment));

		  update_comment(returnvalue, mxmlGetLastChild(comment));
		  mxmlAdd(description, MXML_ADD_AFTER, MXML_ADD_TO_PARENT, mxmlGetLastChild(comment));
                }
		else
		  mxmlDelete(type);

		description = mxmlNewElement(function, "description");

		DEBUG_printf("    adding comment %p/%p to function...\n", mxmlGetLastChild(comment), mxmlGetFirstChild(comment));

		update_comment(function, mxmlGetLastChild(comment));
		mxmlAdd(description, MXML_ADD_AFTER, MXML_ADD_TO_PARENT, mxmlGetLastChild(comment));

		type = NULL;
	      }
	      else if (function && ((ch == ')' && parens == 1) || ch == ','))
	      {
	       /*
	        * Argument definition...
		*/

                if (strcmp(str, "void"))
		{
		  const char *last_string = get_nth_text(type, -1, NULL);

	          mxmlNewText(type, mxmlGetFirstChild(type) != NULL && *last_string != '(' && *last_string != '*', str);

                  DEBUG_printf("Argument: <<<< %s >>>\n", str);

	          variable = add_variable(function, "argument", type);
		}
		else
		  mxmlDelete(type);

		type = NULL;
	      }
              else if (mxmlGetFirstChild(type) && !function && parens == 0 && (ch == ';' || ch == ','))
	      {
	        DEBUG_printf("    got %s, typedefnode=%p, structclass=%p\n", ch == ';' ? "semicolon" : "comma", typedefnode, structclass);
#if DEBUG > 1
		mxml_node_t *debugnode;
		for (debugnode = mxmlGetFirstChild(type); debugnode; debugnode = mxmlGetNextSibling(debugnode))
		  DEBUG_printf("    type child=%p (%s)\n", debugnode, mxmlGetText(debugnode, NULL));
#endif // DEBUG > 1

	        if (typedefnode || structclass)
	        {
                  DEBUG_printf("Typedef/struct/class: <<<< %s >>>>\n", str);

	          if (*str != '_')
		  {
		    if (typedefnode)
		    {
		      if (nsname)
			mxmlElementSetAttrf(typedefnode, "name", "%s::%s", nsname, str);
		      else
			mxmlElementSetAttr(typedefnode, "name", str);

		      sort_node(tree, typedefnode);
		    }

		    if (structclass && !mxmlElementGetAttr(structclass, "name"))
		    {
		      DEBUG_printf("setting struct/class name to \"%s\".\n", get_nth_text(type, -1, NULL));
		      if (nsname)
			mxmlElementSetAttrf(structclass, "name", "%s::%s", nsname, str);
		      else
			mxmlElementSetAttr(structclass, "name", str);

		      sort_node(tree, structclass);
		      structclass = NULL;
		    }

		    if (typedefnode)
		      mxmlAdd(typedefnode, MXML_ADD_BEFORE, MXML_ADD_TO_PARENT, type);
		    else
		      mxmlDelete(type);
		  }

		  type        = NULL;
		  typedefnode = NULL;
		}
		else if (mxmlGetFirstChild(type) && !strcmp(mxmlGetText(mxmlGetFirstChild(type), NULL), "typedef"))
		{
		 /*
		  * Simple typedef...
		  */

                  DEBUG_printf("Simple typedef: <<<< %s >>>\n", str);

		  typedefnode = mxmlNewElement(MXML_NO_PARENT, "typedef");
		  if (nsname)
		    mxmlElementSetAttrf(typedefnode, "name", "%s::%s", nsname, str);
		  else
		    mxmlElementSetAttr(typedefnode, "name", str);
		  mxmlDelete(mxmlGetFirstChild(type));

		  sort_node(tree, typedefnode);

		  if (mxmlGetFirstChild(type))
		    clear_whitespace(mxmlGetFirstChild(type));

		  mxmlAdd(typedefnode, MXML_ADD_AFTER, MXML_ADD_TO_PARENT, type);
		  type = NULL;
		  DEBUG_printf("    New typedefnode=%p\n", typedefnode);
		}
		else if (!parens)
		{
		 /*
	          * Variable definition...
		  */

                  mxml_node_t *child = mxmlGetFirstChild(type);
                  const char *string = get_nth_text(type, -1, NULL);

	          if (child && !strcmp(mxmlGetText(child, NULL), "static") && !strcmp(mxmlGetElement(tree), "codedoc"))
		  {
		   /*
		    * Remove static functions...
		    */

		    mxmlDelete(type);
		    type = NULL;
		    break;
		  }

		  if (nsname && !fstructclass && !tree_is_scu)
		    mxmlNewTextf(type, child != NULL && *string != '(' && *string != '*', "%s::%s", nsname, str);
		  else
		    mxmlNewText(type, child != NULL && *string != '(' && *string != '*', str);

                  DEBUG_printf("Variable: <<<< %s >>>>\n", str);
                  DEBUG_printf("    scope = %s\n", scope ? scope : "(null)");

	          variable = add_variable(MXML_NO_PARENT, "variable", type);
		  type     = NULL;

		  sort_node(tree, variable);

		  if (scope)
		    mxmlElementSetAttr(variable, "scope", scope);
		}
              }
	      else
              {
		mxml_node_t *child = mxmlGetFirstChild(type);
		const char *string = get_nth_text(type, -1, NULL);

                DEBUG_printf("Identifier: <<<< %s >>>>\n", str);

	        mxmlNewText(type, child != NULL && *string != '(' && *string != '*', str);
	      }
	    }
	    else if (enumeration && !isdigit(str[0] & 255))
	    {
	      DEBUG_printf("Constant: <<<< %s >>>\n", str);

	      constant = mxmlNewElement(MXML_NO_PARENT, "constant");
	      if (nsname)
	        mxmlElementSetAttrf(constant, "name", "%s::%s", nsname, str);
	      else
	        mxmlElementSetAttr(constant, "name", str);
	      sort_node(enumeration, constant);
	    }
	    else if (type)
	    {
	      mxmlDelete(type);
	      type = NULL;
	    }
	  }
          break;
    }

#if DEBUG > 1
    if (state != oldstate)
    {
      DEBUG_printf("    changed states from %s to %s on receipt of character '%c'...\n", states[oldstate], states[state], oldch);
      DEBUG_printf("    variable = %p\n", variable);
      DEBUG_printf("    typedefnode = %p\n", typedefnode);
      if (type)
      {
        DEBUG_puts("    type =");
        for (temp = mxmlGetFirstChild(type); temp; temp = mxmlGetNextSibling(temp))
	  DEBUG_printf(" \"%s\"", mxmlGetText(temp, NULL));
	DEBUG_puts("\n");
      }
    }
#endif /* DEBUG > 1 */
  }

  mxmlDelete(comment);

 /*
  * All done, return with no errors...
  */

  return (1);
}


/*
 * 'sort_node()' - Insert a node sorted into a tree.
 */

static void
sort_node(mxml_node_t *tree,		/* I - Tree to sort into */
          mxml_node_t *node)		/* I - Node to add */
{
  mxml_node_t	*temp;			/* Current node */
  const char	*tempname,		/* Name of current node */
		*nodename,		/* Name of node */
		*scope;			/* Scope */


#if DEBUG > 1
  DEBUG_printf("    sort_node(tree=%p, node=%p)\n", tree, node);
#endif /* DEBUG > 1 */

 /*
  * Range check input...
  */

  if (!tree || !node || mxmlGetParent(node) == tree)
    return;

 /*
  * Get the node name...
  */

  if ((nodename = mxmlElementGetAttr(node, "name")) == NULL)
  {
#if DEBUG > 1
    DEBUG_puts("        nodename not found.");
#endif /* DEBUG > 1 */
    return;
  }

  if (nodename[0] == '_')
  {
#if DEBUG > 1
    DEBUG_puts("        nodename=private");
#endif /* DEBUG > 1 */
    return;				/* Hide private names */
  }

#if DEBUG > 1
  DEBUG_printf("        nodename=%p (\"%s\")\n", nodename, nodename);
#endif /* DEBUG > 1 */

 /*
  * Delete any existing definition at this level, if one exists...
  */

  if ((temp = mxmlFindElement(tree, tree, mxmlGetElement(node), "name", nodename, MXML_DESCEND_FIRST)) != NULL)
  {
   /*
    * Copy the scope if needed...
    */

    if ((scope = mxmlElementGetAttr(temp, "scope")) != NULL &&
        mxmlElementGetAttr(node, "scope") == NULL)
    {
      DEBUG_printf("    copying scope %s for %s\n", scope, nodename);

      mxmlElementSetAttr(node, "scope", scope);
    }

    mxmlDelete(temp);
  }

 /*
  * Add the node into the tree at the proper place...
  */

  for (temp = mxmlGetFirstChild(tree); temp; temp = mxmlGetNextSibling(temp))
  {
#if DEBUG > 1
    DEBUG_printf("        temp=%p\n", temp);
#endif /* DEBUG > 1 */

    if ((tempname = mxmlElementGetAttr(temp, "name")) == NULL)
      continue;

#if DEBUG > 1
    DEBUG_printf("        tempname=%p (\"%s\")\n", tempname, tempname);
#endif /* DEBUG > 1 */

    if (strcmp(nodename, tempname) < 0)
      break;
  }

  if (temp)
  {
#if DEBUG > 1
    DEBUG_printf("        adding \"%s\" before \"%s\"\n", nodename, tempname);
#endif /* DEBUG > 1 */

    mxmlAdd(tree, MXML_ADD_BEFORE, temp, node);
  }
  else
  {
#if DEBUG > 1
    DEBUG_printf("        adding \"%s\" to the end\n", nodename);
#endif /* DEBUG > 1 */

    mxmlAdd(tree, MXML_ADD_AFTER, MXML_ADD_TO_PARENT, node);
  }
}


/*
 * 'stringbuf_append()' - Append a Unicode character to a string buffer.
 */

static int				/* O - 1 on success, 0 on failure */
stringbuf_append(stringbuf_t *buffer,	/* I - String buffer */
                 int         ch)	/* I - Character */
{
  if (ch < 0x80)
  {
    if (buffer->bufptr < (buffer->buffer + sizeof(buffer->buffer) - 1))
    {
      *(buffer->bufptr)++ = (char)ch;
      return (1);
    }
  }
  else if (ch < 0x800)
  {
    if (buffer->bufptr < (buffer->buffer + sizeof(buffer->buffer) - 2))
    {
      *(buffer->bufptr)++ = (char)(0xc0 | ((ch >> 6) & 0x1f));
      *(buffer->bufptr)++ = (char)(0x80 | (ch & 0x3f));
      return (1);
    }
  }
  else if (ch < 0x10000)
  {
    if (buffer->bufptr < (buffer->buffer + sizeof(buffer->buffer) - 3))
    {
      *(buffer->bufptr)++ = (char)(0xe0 | ((ch >> 12) & 0x0f));
      *(buffer->bufptr)++ = (char)(0x80 | ((ch >> 6) & 0x3f));
      *(buffer->bufptr)++ = (char)(0x80 | (ch & 0x3f));
      return (1);
    }
  }
  else
  {
    if (buffer->bufptr < (buffer->buffer + sizeof(buffer->buffer) - 4))
    {
      *(buffer->bufptr)++ = (char)(0xf0 | ((ch >> 18) & 0x07));
      *(buffer->bufptr)++ = (char)(0x80 | ((ch >> 12) & 0x3f));
      *(buffer->bufptr)++ = (char)(0x80 | ((ch >> 6) & 0x3f));
      *(buffer->bufptr)++ = (char)(0x80 | (ch & 0x3f));
      return (1);
    }
  }

  return (0);
}


/*
 * 'stringbuf_clear()' - Clear a string buffer.
 */

static void
stringbuf_clear(stringbuf_t *buffer)	/* I - String buffer */
{
  buffer->bufptr = buffer->buffer;
}


/*
 * 'stringbuf_get()' - Get a C string of a string buffer.
 */

static char *				/* O - C string */
stringbuf_get(stringbuf_t *buffer)	/* I - String buffer */
{
  *(buffer->bufptr) = '\0';

  return (buffer->buffer);
}


/*
 * 'stringbuf_getlast()' - Get the last character in a string buffer.
 */

static int				/* O - Last character or EOF */
stringbuf_getlast(stringbuf_t *buffer)	/* I - String buffer */
{
  if (buffer->bufptr > buffer->buffer)
    return (buffer->bufptr[-1]);
  else
    return (EOF);
}


/*
 * 'stringbuf_length()' - Get the length of the string buffer.
 */

static size_t				/* O - Length of string buffer */
stringbuf_length(stringbuf_t *buffer)	/* I - String buffer */
{
  return (buffer->bufptr - buffer->buffer);
}


/*
 * 'update_comment()' - Update a comment node.
 */

static void
update_comment(mxml_node_t *parent,	/* I - Parent node */
               mxml_node_t *comment)	/* I - Comment node */
{
  char	*s,				/* Copy of comment */
  	*ptr;				/* Pointer into comment */


  DEBUG_printf("update_comment(parent=%p, comment=%p)\n", parent, comment);

 /*
  * Range check the input...
  */

  if (!parent || !comment)
    return;

 /*
  * Convert "\/" to "/"...
  */

  s = strdup(mxmlGetOpaque(comment));

  for (ptr = strstr(s, "\\/"); ptr; ptr = strstr(ptr, "\\/"))
    safe_strcpy(ptr, ptr + 1);

 /*
  * Remove leading spaces...
  */

  for (ptr = s; *ptr && isspace(*ptr & 255); ptr ++)
    ;					// Skip leading spaces

  if (ptr > s)
    safe_strcpy(s, ptr);

 /*
  * Update the comment...
  */

  ptr = s;

  if (*ptr == '\'')
  {
   /*
    * Convert "'name()' - description" to "description".
    */

    DEBUG_puts("    found quote");
    for (ptr ++; *ptr && *ptr != '\'' && *ptr != '('; ptr ++)
      ;					// Look for end quote

    if (*ptr == '(' && ptr[1] == ')' && ptr[2] == '\'')
    {
      DEBUG_printf("    comment after quote: %s\n", ptr + 3);
      ptr += 3;
      while (isspace(*ptr & 255))
        ptr ++;

      if (*ptr == '-')
        ptr ++;

      while (isspace(*ptr & 255))
        ptr ++;

      safe_strcpy(s, ptr);
    }
  }
  else if (!strncmp(ptr, "I ", 2) || !strncmp(ptr, "O ", 2) || !strncmp(ptr, "IO ", 3))
  {
   /*
    * 'Convert "I - description", "IO - description", or "O - description"
    * to description + direction attribute.
    */

    ptr = strchr(ptr, ' ');
    *ptr++ = '\0';

    if (!strcmp(mxmlGetElement(parent), "argument"))
      mxmlElementSetAttr(parent, "direction", s);

    while (isspace(*ptr & 255))
      ptr ++;

    if (*ptr == '-')
      ptr ++;

    while (isspace(*ptr & 255))
      ptr ++;

    safe_strcpy(s, ptr);
  }

 /*
  * Eliminate leading and trailing *'s...
  */

  for (ptr = s; *ptr == '*'; ptr ++)
    ;					// Skip leading *

  for (; isspace(*ptr & 255); ptr ++)
    ;					// Skip leading whitespace

  if (ptr > s)
    safe_strcpy(s, ptr);

  for (ptr = s + strlen(s) - 1; ptr > s && *ptr == '*'; ptr --)
    *ptr = '\0';
  for (; ptr > s && isspace(*ptr & 255); ptr --)
    *ptr = '\0';

  DEBUG_printf("    updated comment = %s\n", s);

  mxmlSetOpaque(comment, s);
  free(s);
}


/*
 * 'usage()' - Show program usage...
 */

static void
usage(const char *option)		/* I - Unknown option */
{
  if (option)
    printf("codedoc: Bad option \"%s\".\n\n", option);

  puts("Usage: codedoc [options] [filename.xml] [source files] >filename.html");
  puts("       codedoc [options] [filename.xml] [source files] --epub filename.epub");
  puts("       codedoc [options] [filename.xml] [source files] --man name >name.3");
  puts("");
  puts("Options:");
  puts("    --author \"name\"            Set author name");
  puts("    --body filename            Set body file (markdown supported)");
  puts("    --copyright \"text\"         Set copyright text");
  puts("    --coverimage filename.png  Set cover image (EPUB, HTML)");
  puts("    --css filename.css         Set CSS stylesheet file (EPUB, HTML)");
  puts("    --docversion \"version\"     Set documentation version");
  puts("    --epub filename.epub       Generate EPUB file");
  puts("    --footer filename          Set footer file (markdown supported)");
  puts("    --header filename          Set header file (markdown supported)");
  puts("    --language ll[-LOC]        Set ISO language and locality code (EPUB, HTML)");
  puts("    --man name                 Generate man page");
  puts("    --no-output                Do not generate documentation file");
  puts("    --section \"section\"        Set section name");
  puts("    --title \"title\"            Set documentation title");
  puts("    --version                  Show codedoc version");

  exit(1);
}


/*
 * 'write_description()' - Write the description text.
 */

static void
write_description(
    FILE        *out,			/* I - Output file */
    int         mode,                   /* I - Output mode */
    mxml_node_t *description,		/* I - Description node */
    const char  *element,		/* I - HTML element, if any */
    int         summary)		/* I - Show summary (-1 for all) */
{
  char	text[10240],			/* Text for description */
        *start,				/* Start of code/link */
	*ptr;				/* Pointer into text */
  int	col,				/* Current column */
	list = 0,			/* In a list? */
	bq = 0;				/* In a block quote? */


  if (!description)
    return;

  get_text(description, text, sizeof(text));

  if (summary < 0)
  {
   /*
    * When showing everything, point to the start of the description text...
    */

    ptr = text;
  }
  else
  {
    if ((ptr = strstr(text, "\n\n")) != NULL)
      *ptr = '\0';

    if (summary)
      ptr = text;			/* Summary is the first paragraph */
    else if (!ptr || !ptr[2])
      return;				/* No long-form description */
    else
      ptr += 2;				/* Long-firm description after first */
  }

  if (element && *element)
    fprintf(out, "<%s class=\"%s\">", element, summary ? "description" : "discussion");
  else if (!summary)
    fputs(".PP\n", out);

  for (col = 0; *ptr; ptr ++)
  {
    if (col == 0 && !strncmp(ptr, "- ", 2))
    {
     /*
      * Bullet list item...
      */

      ptr ++;

      if (element)
      {
	if (!list)
	{
          if (!strcmp(element, "p"))
	    fputs("</p>", out);

	  fputs("<ul>\n", out);
	  list = 1;
	}
	else
	  fputs("</li>\n", out);

	fputs("<li>", out);
      }
      else
      {
        list = 1;
        fputs(".IP \\(bu 5\n", out);
      }
    }
    else if (col == 0 && !strncmp(ptr, "> ", 2))
    {
     /*
      * Block quote...
      */

      ptr ++;

      if (element)
      {
	if (!bq)
	{
          if (!strcmp(element, "p"))
	    fputs("</p>", out);

	  fputs("<blockquote>\n", out);
	  bq = 1;
	}
      }
      else
      {
        bq = 1;
        fputs(".IP 5\n", out);
      }
    }
    else if (col == 0 && !strncmp(ptr, "```\n", 4))
    {
     /*
      * Code fence...
      */

      ptr += 4;

      if (element)
        fputs("<pre>\n", out);
      else
        fputs(".nf\n", out);

      while (*ptr && (col != 0 || (strncmp(ptr, "```\n", 4) && strcmp(ptr, "```"))))
      {
        if (col == 0 && !element)
          fputs("    ", out);

        if (col == 0 && *ptr == '|' && ptr[1])
          ptr ++;

	if (*ptr == '\n')
	  col = 0;
	else
	  col ++;

        if (*ptr == '\\' && ptr[1])
          ptr ++;

        if (element)
        {
	  if (*ptr == '&')
	    fputs("&amp;", out);
	  else if (*ptr == '<')
	    fputs("&lt;", out);
	  else if (*ptr == '>')
	    fputs("&gt;", out);
	  else if (*ptr == '\"')
	    fputs("&quot;", out);
	  else if (*ptr & 128)
	  {
	   /*
	    * Convert utf-8 to Unicode constant...
	    */

	    int	ch;			/* Unicode character */


	    ch = *ptr & 255;

	    if ((ch & 0xe0) == 0xc0)
	    {
	      ch = ((ch & 0x1f) << 6) | (ptr[1] & 0x3f);
	      ptr ++;
	    }
	    else if ((ch & 0xf0) == 0xe0)
	    {
	      ch = ((((ch * 0x0f) << 6) | (ptr[1] & 0x3f)) << 6) | (ptr[2] & 0x3f);
	      ptr += 2;
	    }

	    fprintf(out, "&#%d;", ch);
	  }
	  else
	    putc(*ptr, out);
	}
	else
	{
	  if (*ptr == '\\' || ((*ptr == '.' || *ptr == '\'') && col == 0))
	    putc('\\', out);

	  putc(*ptr, out);
	}

	ptr ++;
      }

      if (!strncmp(ptr, "```\n", 4))
        ptr += 3;
      else if (!strcmp(ptr, "```"))
        ptr += 2;

      col = 0;

      if (element)
        fputs("</pre>\n", out);
      else
        fputs("\n.fi\n", out);
    }
    else if (*ptr == '@' && (!strncmp(ptr + 1, "deprecated@", 11) || !strncmp(ptr + 1, "exclude ", 8) || !strncmp(ptr + 1, "since ", 6)))
    {
      ptr ++;
      while (*ptr && *ptr != '@')
        ptr ++;

      if (!*ptr)
        ptr --;
    }
    else if (!strncmp(ptr, "@code ", 6) || *ptr == '`')
    {
      char end = *ptr++;		/* Terminating character */

      if (end != '`')
      {
        for (ptr += 5; isspace(*ptr & 255); ptr ++)
          ;				// Skip whitespace after @code
      }

      for (start = ptr, ptr ++; *ptr && *ptr != end; ptr ++)
        col ++;

      if (*ptr)
        *ptr = '\0';
      else
        ptr --;

      if (element && *element)
      {
        fputs("<code>", out);
        for (; *start; start ++)
        {
          if (*start == '<')
            fputs("&lt;", out);
          else if (*start == '>')
            fputs("&gt;", out);
          else if (*start == '&')
            fputs("&amp;", out);
          else
            putc(*start, out);
        }
        fputs("</code>", out);
      }
      else if (element)
        fputs(start, out);
      else
        fprintf(out, "\\fB%s\\fR", start);
    }
    else if (!strncmp(ptr, "@link ", 6))
    {
      for (ptr += 6; isspace(*ptr & 255); ptr ++)
        ;				// Skip whitespace

      for (start = ptr, ptr ++; *ptr && *ptr != '@'; ptr ++)
        col ++;

      if (*ptr)
        *ptr = '\0';
      else
        ptr --;

      if (element && *element)
        fprintf(out, "<a href=\"#%s\"><code>%s</code></a>", start, start);
      else if (element)
        fputs(start, out);
      else
        fprintf(out, "\\fI%s\\fR", start);
    }
    else if (*ptr == '*' && strchr(ptr + 1, '*'))
    {
     /*
      * Emphasized text...
      */

      int strong = (ptr[1] == '*' && strstr(ptr + 2, "**") != NULL);

      while (*ptr == '*')
        ptr ++;

      for (start = ptr; *ptr && *ptr != '*'; ptr ++)
        col ++;

      if (*ptr)
      {
        *ptr = '\0';

        while (ptr[1] == '*')
          ptr ++;
      }
      else
        ptr --;

      if (element && *element)
      {
        if (strong)
          fprintf(out, "<strong>%s</strong>", start);
        else
          fprintf(out, "<em>%s</em>", start);
      }
      else if (element)
        fputs(start, out);
      else if (strong)
        fprintf(out, "\\fB%s\\fR", start);
      else
        fprintf(out, "\\fI%s\\fR", start);
    }
    else if (*ptr == '[' && strstr(ptr + 1, "](") != NULL)
    {
     /*
      * Link...
      */

      char	*url = strstr(ptr + 1, "]("),
					/* Pointer to URL */
		*urlend = strchr(url + 1, ')');
					/* Pointer to end of URL */
      char	anchor[256];		/* Anchor for @ and @@ links */

      if (!urlend)
      {
        putc('[', out);
        continue;
      }

      start   = ptr + 1;
      *url    = '\0';
      url     += 2;
      *urlend = '\0';

      if (!strcmp(url, "@"))
      {
        // Support @ links to named headings...
        snprintf(anchor, sizeof(anchor), "#%s", markdown_anchor(start));
        url = anchor;
      }
      else if (!strcmp(url, "@@"))
      {
        // Support @@ links to named functions/types/etc...
        snprintf(anchor, sizeof(anchor), "#%s", start);
        url = anchor;
      }

      if (element)
      {
        fprintf(out, "<a href=\"%s\">", url);
        write_string(out, start, mode, 0);
        fputs("</a>", out);
      }
      else
      {
        fprintf(out, "\n.URL %s %s\n", url, start);
      }

      ptr = urlend;
    }
    else if (*ptr == '<' && strchr(ptr + 1, '>') != NULL)
    {
     /*
      * Autolink...
      */

      char	*urlend = strchr(ptr + 1, '>');
					/* Pointer to end of URL */

      start   = ptr + 1;
      *urlend = '\0';

      if (element)
      {
        fprintf(out, "<a href=\"%s\">", start);
        write_string(out, start, mode, 0);
        fputs("</a>", out);
      }
      else
      {
        fprintf(out, "\n.URL %s %s\n", start, start);
      }

      ptr = urlend;
    }
    else if (element)
    {
      if (*ptr == '\n')
      {
        col = 0;
        if (bq && strncmp(ptr + 1, "> ", 2))
        {
          bq = 0;
          fputs("</blockquote>",out);
        }
      }
      else
        col ++;

      if (*ptr == '\\' && ptr[1])
        ptr ++;

      if (*ptr == '&')
        fputs("&amp;", out);
      else if (*ptr == '<')
        fputs("&lt;", out);
      else if (*ptr == '>')
        fputs("&gt;", out);
      else if (*ptr == '\"')
        fputs("&quot;", out);
      else if (*ptr & 128)
      {
       /*
        * Convert utf-8 to Unicode constant...
        */

        int	ch;			/* Unicode character */

        ch = *ptr & 255;

        if ((ch & 0xe0) == 0xc0)
        {
          ch = ((ch & 0x1f) << 6) | (ptr[1] & 0x3f);
	  ptr ++;
        }
        else if ((ch & 0xf0) == 0xe0)
        {
          ch = ((((ch * 0x0f) << 6) | (ptr[1] & 0x3f)) << 6) | (ptr[2] & 0x3f);
	  ptr += 2;
        }

        fprintf(out, "&#%d;", ch);
      }
      else if (*ptr == '\n' && ptr[1] == '\n' && ptr[2] && ptr[2] != '@' && strncmp(ptr + 2, "- ", 2) && strncmp(ptr + 2, "```\n", 4))
      {
        if (list)
        {
          list = 0;
          fputs("</li>\n</ul>\n", out);
          if (!strcmp(element, "p"))
	    fprintf(out, "<%s class=\"%s\">", element, summary ? "description" : "discussion");
        }
        else if (mode == OUTPUT_EPUB)
          fputs("<br />\n<br />\n", out);
        else
          fputs("<br>\n<br>\n", out);
        ptr ++;
      }
      else
        putc(*ptr, out);
    }
    else if (*ptr == '\n' && ptr[1] == '\n' && ptr[2] && ptr[2] != '@')
    {
      list = 0;
      fputs("\n.PP\n", out);
      ptr ++;
      col = 0;
    }
    else
    {
      if (*ptr == '\\' && ptr[1])
        ptr ++;

      if (*ptr == '\\' || ((*ptr == '.' || *ptr == '\'') && col == 0))
        putc('\\', out);

      putc(*ptr, out);

      if (*ptr == '\n')
      {
        col = 0;
        if (bq && strncmp(ptr + 1, "> ", 2))
        {
          bq = 0;
          fputs(".PP\n", out);
        }
      }
      else
        col ++;
    }
  }

  if (list)
    fputs("</li>\n</ul>\n", out);
  else if (bq && element)
    fputs("</blockquote>\n", out);
  else if (element && *element)
  {
    if (summary < 0)
      fprintf(out, "</%s>", element);
    else
      fprintf(out, "</%s>\n", element);
  }
  else if (!element)
    putc('\n', out);
}


/*
 * 'write_element()' - Write an element's text nodes.
 */

static void
write_element(FILE        *out,		/* I - Output file */
              mxml_node_t *doc,		/* I - Document tree */
              mxml_node_t *element,	/* I - Element to write */
              int         mode)		/* I - Output mode */
{
  mxml_node_t	*node;			/* Current node */
  int		whitespace;		/* Current whitespace value */
  const char	*string;		/* Current string value */


  if (!element)
    return;

  for (node = mxmlGetFirstChild(element); node; node = mxmlWalkNext(node, element, MXML_NO_DESCEND))
  {
    if (mxmlGetType(node) == MXML_TEXT)
    {
      string = mxmlGetText(node, &whitespace);

      if (whitespace)
	putc(' ', out);

      if ((mode == OUTPUT_HTML || mode == OUTPUT_EPUB) && (mxmlFindElement(doc, doc, "class", "name", string, MXML_DESCEND) || mxmlFindElement(doc, doc, "enumeration", "name", string, MXML_DESCEND) || mxmlFindElement(doc, doc, "struct", "name", string, MXML_DESCEND) || mxmlFindElement(doc, doc, "typedef", "name", string, MXML_DESCEND) || mxmlFindElement(doc, doc, "union", "name", string, MXML_DESCEND)))
      {
        fputs("<a href=\"#", out);
        write_string(out, string, mode, 0);
	fputs("\">", out);
        write_string(out, string, mode, 0);
	fputs("</a>", out);
      }
      else
        write_string(out, string, mode, 0);
    }
  }

  if (!strcmp(mxmlGetElement(element), "type") && (string = mxmlGetText(mxmlGetLastChild(element), NULL)) != NULL && *string != '*')
    putc(' ', out);
}


/*
 * 'write_epub()' - Write documentation as an EPUB file.
 */

static void
write_epub(const char  *epubfile,	/* I - EPUB file (output) */
           const char  *section,	/* I - Section */
           const char  *title,		/* I - Title */
           const char  *author,		/* I - Author */
           const char  *language,	/* I - Language */
           const char  *copyright,	/* I - Copyright */
           const char  *docversion,	/* I - Document version */
           const char  *cssfile,	/* I - Stylesheet file */
           const char  *coverimage,	/* I - Cover image file */
           const char  *headerfile,	/* I - Header file */
           const char  *bodyfile,	/* I - Body file */
           mmd_t       *body,		/* I - Markdown body */
           mxml_node_t *doc,		/* I - XML documentation */
           const char  *footerfile)	/* I - Footer file */
{
  int		status = 0;		/* Write status */
  size_t	i;			/* Looping var */
  FILE		*fp;			/* Output file */
  char		epubbase[256],		/* Base name of EPUB file (identifier) */
		*epubptr;		/* Pointer into base name */
  zipc_t	*epub;			/* EPUB ZIP container */
  zipc_file_t	*epubf;			/* File in EPUB ZIP container */
  char		xhtmlfile[1024],	/* XHTML output filename */
		*xhtmlptr;		/* Pointer into output filename */
  mxml_node_t	*package_opf,		/* package_opf file */
                *package,		/* package node */
                *metadata,		/* metadata node */
                *manifest,		/* manifest node */
                *spine,			/* spine node */
                *temp;			/* Other (leaf) node */
  char		identifier[256],	/* dc:identifier string */
		*package_opf_string;	/* package_opf file as a string */
  toc_t		*toc;			/* Table of contents */
  toc_entry_t	*tentry;		/* Current table of contents */
  int		toc_level;		/* Current table-of-contents level */
  mmd_t		*node;			/* Current markdown node */
  static const char *mimetype =		/* mimetype file as a string */
		"application/epub+zip";
  static const char *container_xml =	/* container.xml file as a string */
		"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                "<container xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\" version=\"1.0\">\n"
                "  <rootfiles>\n"
                "    <rootfile full-path=\"OEBPS/package.opf\" media-type=\"application/oebps-package+xml\"/>\n"
                "  </rootfiles>\n"
                "</container>\n";


 /*
  * Verify we have everything we need...
  */

  if (coverimage && access(coverimage, R_OK))
  {
    fprintf(stderr, "codedoc: Unable to open cover image \"%s\": %s\n", coverimage, strerror(errno));
    exit(1);
  }

 /*
  * Write the XHTML content...
  */

  strlcpy(xhtmlfile, epubfile, sizeof(xhtmlfile));
  if ((xhtmlptr = strstr(xhtmlfile, ".epub")) != NULL)
    strlcpy(xhtmlptr, ".xhtml", sizeof(xhtmlfile) - (size_t)(xhtmlptr - xhtmlfile));
  else
    strlcat(xhtmlfile, ".xhtml", sizeof(xhtmlfile));

  if ((fp = fopen(xhtmlfile, "w")) == NULL)
  {
    fprintf(stderr, "codedoc: Unable to create temporary XHTML file \"%s\": %s\n", xhtmlfile, strerror(errno));
    exit (1);
  }

 /*
  * Standard header...
  */

  write_html_head(fp, OUTPUT_EPUB, section, title, author, language, copyright, docversion, cssfile);

 /*
  * Header...
  */

  if (headerfile)
  {
   /*
    * Use custom header...
    */

    write_file(fp, headerfile, OUTPUT_EPUB);
  }
  else
  {
   /*
    * Use standard header...
    */

    fputs("<h1 class=\"title\">", fp);
    write_string(fp, title, OUTPUT_EPUB, 0);
    fputs("</h1>\n", fp);

    if (author)
    {
      fputs("<p>", fp);
      write_string(fp, author, OUTPUT_EPUB, 0);
      fputs("</p>\n", fp);
    }

    if (copyright)
    {
      fputs("<p>", fp);
      write_string(fp, copyright, OUTPUT_EPUB, 0);
      fputs("</p>\n", fp);
    }
  }

 /*
  * Body...
  */

  fputs("<div class=\"body\">\n", fp);

  write_html_body(fp, OUTPUT_EPUB, bodyfile, body, doc);

 /*
  * Footer...
  */

  if (footerfile)
  {
   /*
    * Use custom footer...
    */

    write_file(fp, footerfile, OUTPUT_EPUB);
  }

  fputs("</div>\n"
        "</body>\n"
        "</html>\n", fp);

 /*
  * Close XHTML file...
  */

  fclose(fp);

 /*
  * Make the EPUB archive...
  */

  if ((epub = zipcOpen(epubfile, "w")) == NULL)
  {
    fprintf(stderr, "codedoc: Unable to create \"%s\": %s\n", epubfile, strerror(errno));
    unlink(xhtmlfile);
    exit(1);
  }

 /*
  * Add the mimetype file...
  */

  status |= zipcCreateFileWithString(epub, "mimetype", mimetype);

 /*
  * The META-INF/ directory...
  */

  status |= zipcCreateDirectory(epub, "META-INF/");

 /*
  * The META-INF/container.xml file...
  */

  if ((epubf = zipcCreateFile(epub, "META-INF/container.xml", 1)) != NULL)
  {
    status |= zipcFilePuts(epubf, container_xml);
    status |= zipcFileFinish(epubf);
  }
  else
    status = -1;

 /*
  * The OEBPS/ directory...
  */

  status |= zipcCreateDirectory(epub, "OEBPS/");

 /*
  * Copy the OEBPS/body.xhtml file...
  */

  status |= zipcCopyFile(epub, "OEBPS/body.xhtml", xhtmlfile, 1, 1);

  unlink(xhtmlfile);

 /*
  * Add images...
  */

  if (coverimage)
    status |= zipcCopyFile(epub, "OEBPS/cover.png", coverimage, 0, 0);

  node = mmdGetFirstChild(body);

  while (node)
  {
    mmd_t	*next;			/* Next node */
    const char	*url = mmdGetURL(node);	/* URL for node */

    if (mmdGetType(node) == MMD_TYPE_IMAGE && url && strncmp(url, "http://", 7) && strncmp(url, "https://", 8))
    {
      // Image is a local file reference, strip any "#target" and copy it...
      char	filename[1024],		/* Filename */
		*sizespec,		/* Pointer to size specification, if any */
		*name,			/* Base name of file */
		oebpsname[1024];	/* Filename in EPUB file */

      strncpy(filename, url, sizeof(filename) - 1);
      filename[sizeof(filename) - 1] = '\0';

      if ((sizespec = strstr(filename, " =")) != NULL)
        *sizespec = '\0';

      if ((name = strrchr(filename, '/')) != NULL)
        name ++;
      else
        name = filename;

      snprintf(oebpsname, sizeof(oebpsname), "OEBPS/%s", name);

      status |= zipcCopyFile(epub, oebpsname, filename, 0, 0);
    }

    if ((next = mmdGetFirstChild(node)) == NULL)
    {
      if ((next = mmdGetNextSibling(node)) == NULL)
      {
	next = mmdGetParent(node);

	while (next && mmdGetNextSibling(next) == NULL)
	  next = mmdGetParent(next);

	next = mmdGetNextSibling(next);
      }
    }

    node = next;
  }

 /*
  * Now the OEBPS/package.opf file...
  */

  if ((epubptr = strrchr(epubfile, '/')) != NULL)
    strlcpy(epubbase, epubptr + 1, sizeof(epubbase));
  else
    strlcpy(epubbase, epubfile, sizeof(epubbase));

  if ((epubptr = strstr(epubbase, ".epub")) != NULL)
    *epubptr = '\0';

  package_opf = mxmlNewXML("1.0");

  package = mxmlNewElement(package_opf, "package");
  mxmlElementSetAttr(package, "xmlns", "http://www.idpf.org/2007/opf");
  mxmlElementSetAttr(package, "unique-identifier", epubbase);
  mxmlElementSetAttr(package, "version", "3.0");

    metadata = mxmlNewElement(package, "metadata");
    mxmlElementSetAttr(metadata, "xmlns:dc", "http://purl.org/dc/elements/1.1/");
    mxmlElementSetAttr(metadata, "xmlns:opf", "http://www.idpf.org/2007/opf");

      temp = mxmlNewElement(metadata, "dc:title");
      mxmlNewOpaque(temp, title);

      temp = mxmlNewElement(metadata, "dc:creator");
      mxmlNewOpaque(temp, author);

      temp = mxmlNewElement(metadata, "meta");
      mxmlElementSetAttr(temp, "property", "dcterms:modified");
      mxmlNewOpaque(temp, get_iso_date(time(NULL)));

      temp = mxmlNewElement(metadata, "dc:language");
      mxmlNewOpaque(temp, language);

      temp = mxmlNewElement(metadata, "dc:rights");
      mxmlNewOpaque(temp, copyright);

      temp = mxmlNewElement(metadata, "dc:publisher");
      mxmlNewOpaque(temp, "codedoc");

      temp = mxmlNewElement(metadata, "dc:subject");
      mxmlNewOpaque(temp, "Programming");

      temp = mxmlNewElement(metadata, "dc:identifier");
      mxmlElementSetAttr(temp, "id", epubbase);
      snprintf(identifier, sizeof(identifier), "%s-%s", epubbase, docversion);
      mxmlNewOpaque(temp, identifier);

      if (coverimage)
      {
        temp = mxmlNewElement(metadata, "meta");
        mxmlElementSetAttr(temp, "name", "cover");
        mxmlElementSetAttr(temp, "content", "cover-image");
      }

    manifest = mxmlNewElement(package, "manifest");

      temp = mxmlNewElement(manifest, "item");
      mxmlElementSetAttr(temp, "id", "nav");
      mxmlElementSetAttr(temp, "href", "nav.xhtml");
      mxmlElementSetAttr(temp, "media-type", "application/xhtml+xml");
      mxmlElementSetAttr(temp, "properties", "nav");

      temp = mxmlNewElement(manifest, "item");
      mxmlElementSetAttr(temp, "id", "body");
      mxmlElementSetAttr(temp, "href", "body.xhtml");
      mxmlElementSetAttr(temp, "media-type", "application/xhtml+xml");

      if (coverimage)
      {
        temp = mxmlNewElement(manifest, "item");
        mxmlElementSetAttr(temp, "id", "cover-image");
        mxmlElementSetAttr(temp, "href", "cover.png");
        mxmlElementSetAttr(temp, "media-type", "image/png");
      }

    spine = mxmlNewElement(package, "spine");

      temp = mxmlNewElement(spine, "itemref");
      mxmlElementSetAttr(temp, "idref", "body");

  package_opf_string = mxmlSaveAllocString(package_opf, epub_ws_cb);

  if ((epubf = zipcCreateFile(epub, "OEBPS/package.opf", 1)) != NULL)
  {
    status |= zipcFilePuts(epubf, package_opf_string);
    status |= zipcFileFinish(epubf);
  }
  else
    status = -1;

  free(package_opf_string);

 /*
  * Then the OEBPS/nav.xhtml file...
  */

  if ((epubf = zipcCreateFile(epub, "OEBPS/nav.xhtml", 1)) != NULL)
  {
    toc = build_toc(doc, bodyfile, body, footerfile, OUTPUT_EPUB);

    zipcFilePrintf(epubf, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                          "<!DOCTYPE html>\n"
                          "<html xmlns=\"http://www.w3.org/1999/xhtml\" xmlns:epub=\"http://www.idpf.org/2007/ops\">\n"
                          "<head>\n"
                          "<title>%s</title>\n"
                          "<style>ol { list-style-type: none; }</style>\n"
                          "</head>\n"
                          "<body>\n"
                          "<nav epub:type=\"toc\"><ol>\n", title);

    for (i = 0, tentry = toc->entries, toc_level = 1; i < toc->num_entries; i ++, tentry ++)
    {
      if (tentry->level > toc_level)
      {
        toc_level = tentry->level;
      }
      else if (tentry->level < toc_level)
      {
        zipcFilePuts(epubf, "</ol></li>\n");
        toc_level = tentry->level;
      }

      zipcFilePrintf(epubf, "<li><a href=\"body.xhtml#%s\">%s</a>", tentry->anchor, tentry->title);
      if ((i + 1) < toc->num_entries && tentry[1].level > toc_level)
        zipcFilePuts(epubf, "<ol>\n");
      else
        zipcFilePuts(epubf, "</li>\n");
    }

    if (toc_level == 2)
      zipcFilePuts(epubf, "</ol></li>\n");

    zipcFilePuts(epubf, "</ol></nav>\n"
                        "</body>\n"
                        "</html>\n");

    zipcFileFinish(epubf);
    free_toc(toc);
  }
  else
    status = -1;

  status |= zipcClose(epub);

  if (status)
  {
    fprintf(stderr, "codedoc: Unable to write \"%s\": %s\n", epubfile, strerror(errno));
    exit(1);
  }
}


/*
 * 'write_file()' - Copy a file to the output.
 */

static void
write_file(FILE       *out,		/* I - Output file */
           const char *file,		/* I - File to copy */
           int        mode)		/* I - Output mode */
{
  if (is_markdown(file))
  {
   /*
    * Convert markdown source to the output format...
    */

    mmd_t *mmd = mmdLoad(NULL, file);	/* Markdown document */

    if (mmd)
    {
      markdown_write_block(out, mmd, mode);
      mmdFree(mmd);
    }
    else
    {
      fprintf(stderr, "codedoc: Unable to open \"%s\": %s\n", file, strerror(errno));
      exit(1);
    }
  }
  else
  {
   /*
    * Copy non-markdown source...
    */

    FILE	*fp;			/* Copy file */
    char	line[8192],		/* Line from file */
		*ptr;			/* Pointer into line */

    if ((fp = fopen(file, "r")) == NULL)
    {
      fprintf(stderr, "codedoc: Unable to open \"%s\": %s\n", file, strerror(errno));
      exit(1);
    }

    while (fgets(line, sizeof(line), fp))
    {
      if (mode == OUTPUT_EPUB)
      {
       /*
	* Convert common HTML named entities to XHTML numeric entities.
	*/

	for (ptr = line; *ptr; ptr ++)
	{
	  if (!strncmp(ptr, "&nbsp;", 6))
	  {
	    ptr += 5;
	    fputs("&#160;", out);
	  }
	  else if (!strncmp(ptr, "&copy;", 6))
	  {
	    ptr += 5;
	    fputs("&#169;", out);
	  }
	  else if (!strncmp(ptr, "&reg;", 5))
	  {
	    ptr += 4;
	    fputs("&#174;", out);
	  }
	  else if (!strncmp(ptr, "&trade;", 7))
	  {
	    ptr += 6;
	    fputs("&#8482;", out);
	  }
	  else
	    fputc(*ptr, out);
	}
      }
      else
      {
       /*
	* Copy as-is...
	*/

	fputs(line, out);
      }
    }

    fclose(fp);
  }
}


/*
 * 'write_function()' - Write documentation for a function.
 */

static void
write_function(FILE        *out,	/* I - Output file */
               int         mode,	/* I - Output mode */
               mxml_node_t *doc,	/* I - Document */
               mxml_node_t *function,	/* I - Function */
	       int         level)	/* I - Base heading level */
{
  mxml_node_t	*arg,			/* Current argument */
		*adesc,			/* Description of argument */
		*description,		/* Description of function */
		*type,			/* Type for argument */
		*node;			/* Node in description */
  const char	*name,			/* Name of function/type */
		*defval;		/* Default value */
  const char	*prefix;		/* Prefix string */
  char		*sep;			/* Newline separator */


  name        = mxmlElementGetAttr(function, "name");
  description = mxmlFindElement(function, function, "description", NULL, NULL, MXML_DESCEND_FIRST);

  fprintf(out, "<h%d class=\"%s\">%s<a id=\"%s\">%s</a></h%d>\n", level, level == 3 ? "function" : "method", get_comment_info(description), name, name, level);

  if (description)
    write_description(out, mode, description, "p", 1);

  fputs("<p class=\"code\">\n", out);

  arg = mxmlFindElement(function, function, "returnvalue", NULL, NULL, MXML_DESCEND_FIRST);

  if (arg)
    write_element(out, doc, mxmlFindElement(arg, arg, "type", NULL, NULL, MXML_DESCEND_FIRST), OUTPUT_HTML);
  else
    fputs("void ", out);

  fputs(name, out);
  for (arg = mxmlFindElement(function, function, "argument", NULL, NULL, MXML_DESCEND_FIRST), prefix = "("; arg; arg = mxmlFindElement(arg, function, "argument", NULL, NULL, MXML_NO_DESCEND), prefix = ", ")
  {
    type = mxmlFindElement(arg, arg, "type", NULL, NULL, MXML_DESCEND_FIRST);

    fputs(prefix, out);
    if (mxmlGetFirstChild(type))
      write_element(out, doc, type, mode);

    fputs(mxmlElementGetAttr(arg, "name"), out);
    if ((defval = mxmlElementGetAttr(arg, "default")) != NULL)
      fprintf(out, " %s", defval);
  }

  if (!strcmp(prefix, "("))
    fputs("(void);</p>\n", out);
  else
  {
    fprintf(out,
            ");</p>\n"
	    "<h%d class=\"parameters\">Parameters</h%d>\n"
	    "<table class=\"list\"><tbody>\n", level + 1, level + 1);

    for (arg = mxmlFindElement(function, function, "argument", NULL, NULL, MXML_DESCEND_FIRST); arg; arg = mxmlFindElement(arg, function, "argument", NULL, NULL, MXML_NO_DESCEND))
    {
      fprintf(out, "<tr><th>%s</th>\n", mxmlElementGetAttr(arg, "name"));

      adesc = mxmlFindElement(arg, arg, "description", NULL, NULL, MXML_DESCEND_FIRST);

      write_description(out, mode, adesc, "td", -1);
      fputs("</tr>\n", out);
    }

    fputs("</tbody></table>\n", out);
  }

  arg = mxmlFindElement(function, function, "returnvalue", NULL, NULL, MXML_DESCEND_FIRST);

  if (arg)
  {
    fprintf(out, "<h%d class=\"returnvalue\">Return Value</h%d>\n", level + 1, level + 1);

    adesc = mxmlFindElement(arg, arg, "description", NULL, NULL, MXML_DESCEND_FIRST);

    write_description(out, mode, adesc, "p", 1);
    write_description(out, mode, adesc, "p", 0);
  }

  if (description)
  {
    for (node = mxmlGetFirstChild(description); node; node = mxmlGetNextSibling(node))
    {
      const char *opaque = mxmlGetOpaque(node);

      if (opaque && (sep = strstr(opaque, "\n\n")) != NULL)
      {
	sep += 2;
	if (*sep && strncmp(sep, "@since ", 7) && strncmp(sep, "@deprecated@", 12))
	  break;
      }
    }

    if (node)
    {
      fprintf(out, "<h%d class=\"discussion\">Discussion</h%d>\n", level + 1, level + 1);
      write_description(out, mode, description, "p", 0);
    }
  }
}


/*
 * 'write_html()' - Write HTML documentation.
 */

static void
write_html(const char  *section,	/* I - Section */
	   const char  *title,		/* I - Title */
           const char  *author,		/* I - Author's name */
           const char  *language,	/* I - Language */
           const char  *copyright,	/* I - Copyright string */
	   const char  *docversion,	/* I - Documentation set version */
	   const char  *cssfile,	/* I - Stylesheet file */
           const char  *coverimage,	/* I - Cover image file */
	   const char  *headerfile,	/* I - Header file */
	   const char  *bodyfile,	/* I - Body file */
           mmd_t       *body,		/* I - Markdown body */
	   mxml_node_t *doc,		/* I - XML documentation */
           const char  *footerfile)	/* I - Footer file */
{
  toc_t		*toc;			/* Table of contents */


 /*
  * Create the table-of-contents entries...
  */

  toc = build_toc(doc, bodyfile, body, footerfile, OUTPUT_HTML);

 /*
  * Standard header...
  */

  write_html_head(stdout, OUTPUT_HTML, section, title, author, language, copyright, docversion, cssfile);

  puts("<div class=\"header\">");

  if (coverimage)
  {
    const char	*coverbase;		/* Base name for cover image */

    if ((coverbase = strrchr(coverimage, '/')) != NULL)
      coverbase ++;
    else
      coverbase = coverimage;

    fputs("<p><img class=\"title\" src=\"", stdout);
    write_string(stdout, coverbase, OUTPUT_HTML, 0);
    fputs("\"></p>\n", stdout);
  }

 /*
  * Header...
  */

  if (headerfile)
  {
   /*
    * Use custom header...
    */

    write_file(stdout, headerfile, OUTPUT_HTML);
  }
  else
  {
   /*
    * Use standard header...
    */

    fputs("<h1 class=\"title\">", stdout);
    write_string(stdout, title, OUTPUT_HTML, 0);
    fputs("</h1>\n", stdout);

    if (author)
    {
      fputs("<p>", stdout);
      write_string(stdout, author, OUTPUT_HTML, 0);
      fputs("</p>\n", stdout);
    }

    if (copyright)
    {
      fputs("<p>", stdout);
      write_string(stdout, copyright, OUTPUT_HTML, 0);
      fputs("</p>\n", stdout);
    }
  }

  puts("</div>");

 /*
  * Table of contents...
  */

  write_html_toc(stdout, title, toc, NULL, NULL);

  free_toc(toc);

 /*
  * Body...
  */

  puts("<div class=\"body\">");

  write_html_body(stdout, OUTPUT_HTML, bodyfile, body, doc);

 /*
  * Footer...
  */

  if (footerfile)
  {
   /*
    * Use custom footer...
    */

    puts("</div>");
    puts("<div class=\"footer\">");

    write_file(stdout, footerfile, OUTPUT_HTML);
  }

  puts("</div>\n"
       "</body>\n"
       "</html>");
}


/*
 * 'write_html_body()' - Write a HTML/XHTML body.
 */

static void
write_html_body(
    FILE        *out,			/* I - Output file */
    int         mode,			/* I - HTML or EPUB/XHTML output */
    const char  *bodyfile,		/* I - Body file */
    mmd_t       *body,			/* I - Markdown body */
    mxml_node_t *doc)			/* I - XML documentation */
{
  mxml_node_t	*function,		/* Current function */
		*scut,			/* Struct/class/union/typedef */
		*arg,			/* Current argument */
		*description,		/* Description of function/var */
		*type;			/* Type for argument */
  const char	*name,			/* Name of function/type */
		*defval;		/* Default value */
  int		whitespace;		/* Current whitespace value */
  const char	*string;		/* Current string value */


 /*
  * Body...
  */

  if (body)
    markdown_write_block(out, body, mode);
  else if (bodyfile)
    write_file(out, bodyfile, mode);

 /*
  * List of classes...
  */

  if ((scut = find_public(doc, doc, "class", NULL, mode)) != NULL)
  {
    fputs("<h2 class=\"title\"><a id=\"CLASSES\">Classes</a></h2>\n", out);

    while (scut)
    {
      write_scu(out, mode, doc, scut);

      scut = find_public(scut, doc, "class", NULL, mode);
    }
  }

 /*
  * List of functions...
  */

  if ((function = find_public(doc, doc, "function", NULL, mode)) != NULL)
  {
    fputs("<h2 class=\"title\"><a id=\"FUNCTIONS\">Functions</a></h2>\n", out);

    while (function)
    {
      write_function(out, mode, doc, function, 3);

      function = find_public(function, doc, "function", NULL, mode);
    }
  }

 /*
  * List of types...
  */

  if ((scut = find_public(doc, doc, "typedef", NULL, mode)) != NULL)
  {
    fputs("<h2 class=\"title\"><a id=\"TYPES\">Data Types</a></h2>\n", out);

    while (scut)
    {
      name        = mxmlElementGetAttr(scut, "name");
      description = mxmlFindElement(scut, scut, "description", NULL, NULL, MXML_DESCEND_FIRST);
      fprintf(out, "<h3 class=\"typedef\"><a id=\"%s\">%s%s</a></h3>\n", name, get_comment_info(description), name);

      if (description)
	write_description(out, mode, description, "p", 1);

      fputs("<p class=\"code\">\n"
	    "typedef ", out);

      type = mxmlFindElement(scut, scut, "type", NULL, NULL, MXML_DESCEND_FIRST);

      for (type = mxmlGetFirstChild(type); type; type = mxmlGetNextSibling(type))
      {
        string = mxmlGetText(type, &whitespace);

        if (!strcmp(string, "("))
        {
	  break;
	}
	else
	{
	  if (whitespace)
	    putc(' ', out);

	  if (find_public(doc, doc, "class", string, mode) || find_public(doc, doc, "enumeration", string, mode) || find_public(doc, doc, "struct", string, mode) || find_public(doc, doc, "typedef", string, mode) || find_public(doc, doc, "union", string, mode))
	  {
            fputs("<a href=\"#", out);
            write_string(out, string, OUTPUT_HTML, 0);
	    fputs("\">", out);
            write_string(out, string, OUTPUT_HTML, 0);
	    fputs("</a>", out);
	  }
	  else
            write_string(out, string, OUTPUT_HTML, 0);
        }
      }

      if (type)
      {
       /*
        * Output function type...
	*/

        string = mxmlGetText(mxmlGetPrevSibling(type), NULL);

        if (string && *string != '*')
	  putc(' ', out);

        fprintf(out, "(*%s", name);

	for (type = mxmlGetNextSibling(mxmlGetNextSibling(type)); type; type = mxmlGetNextSibling(type))
	{
	  string = mxmlGetText(type, &whitespace);

	  if (whitespace)
	    putc(' ', out);

	  if (find_public(doc, doc, "class", string, mode) || find_public(doc, doc, "enumeration", string, mode) || find_public(doc, doc, "struct", string, mode) || find_public(doc, doc, "typedef", string, mode) || find_public(doc, doc, "union", string, mode))
	  {
            fputs("<a href=\"#", out);
            write_string(out, string, OUTPUT_HTML, 0);
	    fputs("\">", out);
            write_string(out, string, OUTPUT_HTML, 0);
	    fputs("</a>", out);
	  }
	  else
            write_string(out, string, OUTPUT_HTML, 0);
        }

        fputs(";\n", out);
      }
      else
      {
	type   = mxmlFindElement(scut, scut, "type", NULL, NULL, MXML_DESCEND_FIRST);
	string = mxmlGetText(mxmlGetLastChild(type), NULL);

        if (*string != '*')
	  putc(' ', out);

	fprintf(out, "%s;\n", name);
      }

      fputs("</p>\n", out);

      scut = find_public(scut, doc, "typedef", NULL, mode);
    }
  }

 /*
  * List of structures...
  */

  if ((scut = find_public(doc, doc, "struct", NULL, mode)) != NULL)
  {
    fputs("<h2 class=\"title\"><a id=\"STRUCTURES\">Structures</a></h2>\n", out);

    while (scut)
    {
      write_scu(out, mode, doc, scut);

      scut = find_public(scut, doc, "struct", NULL, mode);
    }
  }

 /*
  * List of unions...
  */

  if ((scut = find_public(doc, doc, "union", NULL, mode)) != NULL)
  {
    fputs("<h2 class=\"title\"><a id=\"UNIONS\">Unions</a></h2>\n", out);

    while (scut)
    {
      write_scu(out, mode, doc, scut);

      scut = find_public(scut, doc, "union", NULL, mode);
    }
  }

 /*
  * Variables...
  */

  if ((arg = find_public(doc, doc, "variable", NULL, mode)) != NULL)
  {
    fputs("<h2 class=\"title\"><a id=\"VARIABLES\">Variables</a></h2>\n", out);

    while (arg)
    {
      name        = mxmlElementGetAttr(arg, "name");
      description = mxmlFindElement(arg, arg, "description", NULL, NULL, MXML_DESCEND_FIRST);
      fprintf(out, "<h3 class=\"variable\"><a id=\"%s\">%s%s</a></h3>\n", name, get_comment_info(description), name);

      if (description)
	write_description(out, mode, description, "p", 1);

      fputs("<p class=\"code\">", out);

      write_element(out, doc, mxmlFindElement(arg, arg, "type", NULL, NULL, MXML_DESCEND_FIRST), OUTPUT_HTML);
      fputs(mxmlElementGetAttr(arg, "name"), out);
      if ((defval = mxmlElementGetAttr(arg, "default")) != NULL)
	fprintf(out, " %s", defval);
      fputs(";</p>\n", out);

      arg = find_public(arg, doc, "variable", NULL, mode);
    }
  }

 /*
  * List of enumerations...
  */

  if ((scut = find_public(doc, doc, "enumeration", NULL, mode)) != NULL)
  {
    fputs("<h2 class=\"title\"><a id=\"ENUMERATIONS\">Constants</a></h2>\n", out);

    while (scut)
    {
      name        = mxmlElementGetAttr(scut, "name");
      description = mxmlFindElement(scut, scut, "description", NULL, NULL, MXML_DESCEND_FIRST);
      fprintf(out, "<h3 class=\"enumeration\"><a id=\"%s\">%s%s</a></h3>\n", name, get_comment_info(description), name);

      if (description)
	write_description(out, mode, description, "p", 1);

      fputs("<h4 class=\"constants\">Constants</h4>\n"
            "<table class=\"list\"><tbody>\n", out);

      for (arg = find_public(scut, scut, "constant", NULL, mode); arg; arg = find_public(arg, scut, "constant", NULL, mode))
      {
	description = mxmlFindElement(arg, arg, "description", NULL, NULL, MXML_DESCEND_FIRST);
	fprintf(out, "<tr><th>%s %s</th>", mxmlElementGetAttr(arg, "name"), get_comment_info(description));

	write_description(out, mode, description, "td", -1);
        fputs("</tr>\n", out);
      }

      fputs("</tbody></table>\n", out);

      scut = find_public(scut, doc, "enumeration", NULL, mode);
    }
  }
}


/*
 * 'write_html_head()' - Write the standard HTML header.
 */

static void
write_html_head(FILE       *out,	/* I - Output file */
                int        mode,	/* I - HTML or EPUB/XHTML */
                const char *section,	/* I - Section */
                const char *title,	/* I - Title */
                const char *author,	/* I - Author's name */
                const char *language,	/* I - Language */
                const char *copyright,	/* I - Copyright string */
                const char *docversion,	/* I - Document version string */
		const char *cssfile)	/* I - Stylesheet */
{
  if (mode == OUTPUT_EPUB)
    fprintf(out, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
		 "<!DOCTYPE html>\n"
		 "<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"%s\" "
		 "lang=\"%s\">\n", language, language);
  else
    fprintf(out, "<!DOCTYPE html>\n"
		 "<html lang=\"%s\">\n", language);

  if (section)
    fprintf(out, "<!-- SECTION: %s -->\n", section);

  fputs("<head>\n"
        "<title>", out);
  write_string(out, title, mode, 0);
  fputs("</title>\n", out);

  if (mode == OUTPUT_EPUB)
  {
    if (section)
      fprintf(out, "<meta name=\"keywords\" content=\"%s\" />\n", section);

    fputs("<meta name=\"generator\" content=\"codedoc v" VERSION "\" />\n"
          "<meta name=\"author\" content=\"", out);
    write_string(out, author, mode, 0);
    fprintf(out, "\" />\n"
		 "<meta name=\"language\" content=\"%s\" />\n"
		 "<meta name=\"copyright\" content=\"", language);
    write_string(out, copyright, mode, 0);
    fputs("\" />\n"
          "<meta name=\"version\" content=\"", out);
    write_string(out, docversion, mode, 0);
    fputs("\" />\n"
          "<style type=\"text/css\"><![CDATA[\n", out);
  }
  else
  {
    if (section)
      fprintf(out, "<meta name=\"keywords\" content=\"%s\">\n", section);

    fputs("<meta http-equiv=\"Content-Type\" content=\"text/html;charset=utf-8\">\n"
          "<meta name=\"generator\" content=\"codedoc v" VERSION "\">\n"
          "<meta name=\"author\" content=\"", out);
    write_string(out, author, mode, 0);
    fprintf(out, "\">\n"
		 "<meta name=\"language\" content=\"%s\">\n"
		 "<meta name=\"copyright\" content=\"", language);
    write_string(out, copyright, mode, 0);
    fputs("\">\n"
          "<meta name=\"version\" content=\"", out);
    write_string(out, docversion, mode, 0);
    fputs("\">\n"
          "<style type=\"text/css\"><!--\n", out);
  }

  if (cssfile)
  {
   /*
    * Use custom stylesheet file...
    */

    write_file(out, cssfile, mode);
  }
  else
  {
   /*
    * Use standard stylesheet...
    */

    fputs("body {\n"
          "  background: white;\n"
          "  color: black;\n"
          "  font-family: sans-serif;\n"
          "  font-size: 12pt;\n"
          "}\n"
          "a {\n"
          "  color: black;\n"
          "}\n"
          "a:link, a:visited {\n"
          "  color: #00f;\n"
          "}\n"
          "a:link:hover, a:visited:hover, a:active {\n"
          "  color: #c0c;\n"
          "}\n"
          "body, p, h1, h2, h3, h4, h5, h6 {\n"
	  "  font-family: sans-serif;\n"
	  "  line-height: 1.4;\n"
	  "}\n"
	  "h1, h2, h3, h4, h5, h6 {\n"
	  "  font-weight: bold;\n"
	  "  page-break-inside: avoid;\n"
	  "}\n"
	  "h1 {\n"
	  "  font-size: 250%;\n"
	  "  margin: 0;\n"
	  "}\n"
	  "h2 {\n"
	  "  font-size: 250%;\n"
	  "  margin-top: 1.5em;\n"
	  "}\n"
	  "h3 {\n"
	  "  font-size: 200%;\n"
	  "  margin-bottom: 0.5em;\n"
	  "  margin-top: 1.5em;\n"
	  "}\n"
	  "h4 {\n"
	  "  font-size: 150%;\n"
	  "  margin-bottom: 0.5em;\n"
	  "  margin-top: 1.5em;\n"
	  "}\n"
	  "h5 {\n"
	  "  font-size: 125%;\n"
	  "  margin-bottom: 0.5em;\n"
	  "  margin-top: 1.5em;\n"
	  "}\n"
	  "h6 {\n"
	  "  font-size: 110%;\n"
	  "  margin-bottom: 0.5em;\n"
	  "  margin-top: 1.5em;\n"
	  "}\n"
	  "img.title {\n"
	  "  width: 256px;\n"
	  "}\n"
	  "div.header h1, div.header p {\n"
	  "  text-align: center;\n"
	  "}\n"
	  "div.contents, div.body, div.footer {\n"
	  "  page-break-before: always;\n"
	  "}\n"
	  ".class, .enumeration, .function, .struct, .typedef, .union {\n"
	  "  border-bottom: solid 2px gray;\n"
	  "}\n"
	  ".description {\n"
	  "  margin-top: 0.5em;\n"
	  "}\n"
	  ".function {\n"
	  "  margin-bottom: 0;\n"
	  "}\n"
	  "blockquote {\n"
	  "  border: solid thin gray;\n"
	  "  box-shadow: 3px 3px 5px rgba(127,127,127,0.25);\n"
	  "  margin: 1em 0;\n"
	  "  padding: 10px;\n"
	  "  page-break-inside: avoid;\n"
          "}\n"
	  "p code, li code, p.code, pre, ul.code li {\n"
	  "  font-family: monospace;\n"
	  "  hyphens: manual;\n"
	  "  -webkit-hyphens: manual;\n"
	  "}\n"
	  "p.code, pre, ul.code li {\n"
          "  background: rgba(127,127,127,0.25);\n"
          "  border: thin dotted gray;\n"
          "  padding: 10px;\n"
	  "  page-break-inside: avoid;\n"
	  "}\n"
	  "pre {\n"
	  "  white-space: pre-wrap;\n"
	  "}\n"
	  "a:link, a:visited {\n"
	  "  text-decoration: none;\n"
	  "}\n"
	  "span.info {\n"
	  "  background: black;\n"
	  "  border: solid thin black;\n"
	  "  color: white;\n"
	  "  font-size: 80%;\n"
	  "  font-style: italic;\n"
	  "  font-weight: bold;\n"
	  "  white-space: nowrap;\n"
	  "}\n"
	  "h1 span.info, h2 span.info, h3 span.info, h4 span.info {\n"
	  "  border-top-left-radius: 10px;\n"
	  "  border-top-right-radius: 10px;\n"
	  "  float: right;\n"
	  "  padding: 3px 6px;\n"
	  "}\n"
	  "ul.code, ul.contents, ul.subcontents {\n"
	  "  list-style-type: none;\n"
	  "  margin: 0;\n"
	  "  padding-left: 0;\n"
	  "}\n"
	  "ul.code li {\n"
	  "  margin: 0;\n"
	  "}\n"
	  "ul.contents > li {\n"
	  "  margin-top: 1em;\n"
	  "}\n"
	  "ul.contents li ul.code, ul.contents li ul.subcontents {\n"
	  "  padding-left: 2em;\n"
	  "}\n"
	  "table {\n"
	  "  border-collapse: collapse;\n"
	  "  border-spacing: 0;\n"
	  "}\n"
	  "td {\n"
	  "  border: solid 1px gray;\n"
	  "  padding: 5px 10px;\n"
	  "  vertical-align: top;\n"
	  "}\n"
	  "td.left {\n"
	  "  text-align: left;\n"
	  "}\n"
	  "td.center {\n"
	  "  text-align: center;\n"
	  "}\n"
	  "td.right {\n"
	  "  text-align: right;\n"
	  "}\n"
	  "th {\n"
	  "  border-bottom: solid 2px gray;\n"
	  "  padding: 1px 5px;\n"
	  "  text-align: center;\n"
	  "  vertical-align: bottom;\n"
	  "}\n"
	  "tr:nth-child(even) {\n"
	  "  background: rgba(127,127,127,0.25);\n"
	  "}\n"
	  "table.list {\n"
	  "  border-collapse: collapse;\n"
	  "  width: 100%;\n"
	  "}\n"
	  "table.list th {\n"
	  "  border-bottom: none;\n"
	  "  border-right: 2px solid gray;\n"
	  "  font-family: monospace;\n"
	  "  font-weight: normal;\n"
	  "  padding: 5px 10px 5px 2px;\n"
	  "  text-align: right;\n"
	  "  vertical-align: top;\n"
	  "}\n"
	  "table.list td {\n"
	  "  border: none;\n"
	  "  padding: 5px 2px 5px 10px;\n"
	  "  text-align: left;\n"
	  "  vertical-align: top;\n"
	  "}\n"
	  "h2.title, h3.title {\n"
	  "  border-bottom: solid 2px gray;\n"
	  "}\n"
	  "/* Syntax highlighting */\n"
	  "span.comment {\n"
	  "  color: darkgreen;\n"
	  "}\n"
	  "span.directive {\n"
	  "  color: purple;\n"
	  "}\n"
	  "span.number {\n"
	  "  color: brown;\n"
	  "}\n"
	  "span.reserved {\n"
	  "  color: darkcyan;\n"
	  "}\n"
	  "span.string {\n"
	  "  color: magenta;\n"
	  "}\n"
	  "/* Dark mode overrides */\n"
	  "@media (prefers-color-scheme: dark) {\n"
	  "  body {\n"
	  "    background: black;\n"
	  "    color: #ccc;\n"
	  "  }\n"
	  "  a {\n"
	  "    color: #ccc;\n"
	  "  }\n"
	  "  a:link, a:visited {\n"
	  "    color: #66f;\n"
	  "  }\n"
	  "  a:link:hover, a:visited:hover, a:active {\n"
	  "    color: #f06;\n"
	  "  }\n"
	  "}\n", out);

    if (mode == OUTPUT_HTML)
      fputs("/* Show contents on left side in web browser */\n"
            "@media screen and (min-width: 800px) {\n"
            "  div.contents {\n"
            "    border-right: solid thin gray;\n"
            "    bottom: 0px;\n"
            "    box-shadow: 3px 3px 5px rgba(127,127,127,0.5);\n"
            "    font-size: 10pt;\n"
            "    left: 0px;\n"
            "    overflow: scroll;\n"
            "    padding: 1%;\n"
            "    position: fixed;\n"
            "    top: 0px;\n"
            "    width: 18%;\n"
            "  }\n"
            "  div.contents h2.title {\n"
            "    margin-top: 0px;\n"
            "  }\n"
            "  div.header, div.body, div.footer {\n"
            "    margin-left: 20%;\n"
            "    padding: 1% 2%;\n"
            "  }\n"
            "}\n"
            "/* Center title page content vertically */\n"
            "@media print {\n"
            "  div.header {\n"
            "    padding-top: 33%;\n"
            "  }\n"
            "}\n", out);
  }

  if (mode == OUTPUT_EPUB)
    fputs("]]></style>\n"
          "</head>\n"
          "<body>\n", out);
  else
    fputs("--></style>\n"
          "</head>\n"
          "<body>\n", out);
}


/*
 * 'write_html_toc()' - Write a HTML table-of-contents.
 */

static void
write_html_toc(FILE        *out,	/* I - Output file */
               const char  *title,	/* I - Title */
               toc_t       *toc,	/* I - Table of contents */
               const char  *filename,	/* I - Target filename, if any */
	       const char  *target)	/* I - Target frame name, if any */
{
  size_t	i;			/* Looping var */
  toc_entry_t	*tentry;		/* Current table of contents */
  int		toc_level;		/* Current table-of-contents level */
  char		targetattr[1024];	/* Target attribute, if any */


 /*
  * If target is set, it is the frame file that contains the body.
  * Otherwise, we are creating a single-file...
  */

  if (target)
    snprintf(targetattr, sizeof(targetattr), " target=\"%s\"", target);
  else
    targetattr[0] = '\0';

  fputs("<div class=\"contents\">\n", out);

  if (filename)
  {
    fprintf(out, "<h1 class=\"title\"><a href=\"%s\"%s>", filename, targetattr);
    write_string(out, title, OUTPUT_HTML, 0);
    fputs("</a></h1>\n", out);
  }

  fputs("<h2 class=\"title\">Contents</h2>\n"
        "<ul class=\"contents\">\n", out);

  for (i = 0, tentry = toc->entries, toc_level = 1; i < toc->num_entries; i ++, tentry ++)
  {
    if (tentry->level > toc_level)
    {
      toc_level = tentry->level;
    }
    else if (tentry->level < toc_level)
    {
      fputs("</ul></li>\n", out);
      toc_level = tentry->level;
    }

    fprintf(out, "<li><a href=\"%s#%s\"%s>", filename ? filename : "", tentry->anchor, targetattr);
    write_string(out, tentry->title, OUTPUT_HTML, 0);

    if ((i + 1) < toc->num_entries && tentry[1].level > toc_level)
      fputs("</a><ul class=\"subcontents\">\n", out);
    else
      fputs("</a></li>\n", out);
  }

  if (toc_level == 2)
    fputs("</ul></li>\n", out);

  fputs("</ul>\n"
        "</div>\n", out);
}


/*
 * 'write_man()' - Write manpage documentation.
 */

static void
write_man(const char  *man_name,	/* I - Name of manpage */
	  const char  *section,		/* I - Section */
	  const char  *title,		/* I - Title */
          const char  *author,		/* I - Author's name */
          const char  *copyright,	/* I - Copyright string */
	  const char  *headerfile,	/* I - Header file */
	  const char  *bodyfile,	/* I - Body file */
          mmd_t       *body,		/* I - Markdown body */
	  mxml_node_t *doc,		/* I - XML documentation */
	  const char  *footerfile)	/* I - Footer file */
{
  int		i;			/* Looping var */
  mxml_node_t	*function,		/* Current function */
		*scut,			/* Struct/class/union/typedef */
		*arg,			/* Current argument */
		*description,		/* Description of function/var */
		*type;			/* Type for argument */
  const char	*name,			/* Name of function/type */
		*cname,			/* Class name */
		*defval,		/* Default value */
		*parent;		/* Parent class */
  int		inscope;		/* Variable/method scope */
  char		prefix;			/* Prefix character */
  const char	*source_date_epoch;	/* SOURCE_DATE_EPOCH environment variable */
  time_t	curtime;		/* Current time */
  struct tm	curdate;		/* Current date */
  char		buffer[1024];		/* String buffer */
  int		whitespace;		/* Current whitespace value */
  const char	*string;		/* Current string value */
  static const char * const scopes[] =	/* Scope strings */
		{
		  "private",
		  "protected",
		  "public"
		};


 /*
  * Standard man page...
  *
  * Get the current date, using the SOURCE_DATE_EPOCH environment variable, if
  * present, for the number of seconds since the epoch - this enables
  * reproducible builds (Mini-XML Issue #193).
  */

  if ((source_date_epoch = getenv("SOURCE_DATE_EPOCH")) == NULL || (curtime = (time_t)strtol(source_date_epoch, NULL, 10)) <= 0)
    curtime = time(NULL);

  localtime_r(&curtime, &curdate);
  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", curdate.tm_year + 1900, curdate.tm_mon + 1, curdate.tm_mday);

  printf(".TH %s %s \"%s\" \"%s\" \"%s\"\n", man_name, section ? section : "3",
         title ? title : "", buffer, title ? title : "");

 /*
  * Header...
  */

  if (headerfile)
  {
   /*
    * Use custom header...
    */

    write_file(stdout, headerfile, OUTPUT_MAN);
  }
  else
  {
   /*
    * Use standard header...
    */

    puts(".SH NAME");
    printf("%s \\- %s\n", man_name, title ? title : man_name);
  }

 /*
  * Body...
  */

  if (body)
    markdown_write_block(stdout, body, OUTPUT_MAN);
  else if (bodyfile)
    write_file(stdout, bodyfile, OUTPUT_MAN);

 /*
  * List of classes...
  */

  if (find_public(doc, doc, "class", NULL, OUTPUT_MAN))
  {
    puts(".SH CLASSES");

    for (scut = find_public(doc, doc, "class", NULL, OUTPUT_MAN); scut; scut = find_public(scut, doc, "class", NULL, OUTPUT_MAN))
    {
      cname       = mxmlElementGetAttr(scut, "name");
      description = mxmlFindElement(scut, scut, "description", NULL, NULL, MXML_DESCEND_FIRST);
      printf(".SS %s\n", cname);

      write_description(stdout, OUTPUT_MAN, description, NULL, 1);

      printf(".PP\n"
             ".nf\n"
             "class %s", cname);
      if ((parent = mxmlElementGetAttr(scut, "parent")) != NULL)
        printf(" %s", parent);
      puts("\n{");

      for (i = 0; i < 3; i ++)
      {
        inscope = 0;

	for (arg = mxmlFindElement(scut, scut, "variable", "scope", scopes[i], MXML_DESCEND_FIRST); arg; arg = mxmlFindElement(arg, scut, "variable", "scope", scopes[i], MXML_NO_DESCEND))
	{
          if (!inscope)
	  {
	    inscope = 1;
	    printf("  %s:\n", scopes[i]);
	  }

	  printf("    ");
	  write_element(stdout, doc, mxmlFindElement(arg, arg, "type", NULL, NULL, MXML_DESCEND_FIRST), OUTPUT_MAN);
	  printf("%s;\n", mxmlElementGetAttr(arg, "name"));
	}

	for (function = mxmlFindElement(scut, scut, "function", "scope", scopes[i], MXML_DESCEND_FIRST); function; function = mxmlFindElement(function, scut, "function", "scope", scopes[i], MXML_NO_DESCEND))
	{
          if (!inscope)
	  {
	    inscope = 1;
	    printf("  %s:\n", scopes[i]);
	  }

          name = mxmlElementGetAttr(function, "name");

          printf("    ");

	  arg = mxmlFindElement(function, function, "returnvalue", NULL, NULL, MXML_DESCEND_FIRST);

	  if (arg)
	    write_element(stdout, doc, mxmlFindElement(arg, arg, "type", NULL, NULL, MXML_DESCEND_FIRST), OUTPUT_MAN);
	  else if (strcmp(cname, name) && strcmp(cname, name + 1))
	    fputs("void ", stdout);

	  printf("%s", name);

	  for (arg = mxmlFindElement(function, function, "argument", NULL, NULL, MXML_DESCEND_FIRST), prefix = '('; arg; arg = mxmlFindElement(arg, function, "argument", NULL, NULL, MXML_NO_DESCEND), prefix = ',')
	  {
	    type = mxmlFindElement(arg, arg, "type", NULL, NULL, MXML_DESCEND_FIRST);

	    putchar(prefix);
	    if (prefix == ',')
	      putchar(' ');

	    if (mxmlGetFirstChild(type))
	      write_element(stdout, doc, type, OUTPUT_MAN);
	    fputs(mxmlElementGetAttr(arg, "name"), stdout);
            if ((defval = mxmlElementGetAttr(arg, "default")) != NULL)
	      printf(" %s", defval);
	  }

	  if (prefix == '(')
	    puts("(void);");
	  else
	    puts(");");
	}
      }

      puts("};\n"
           ".fi");

      write_description(stdout, OUTPUT_MAN, description, NULL, 0);
    }
  }

 /*
  * List of enumerations...
  */

  if (find_public(doc, doc, "enumeration", NULL, OUTPUT_MAN))
  {
    puts(".SH ENUMERATIONS");

    for (scut = find_public(doc, doc, "enumeration", NULL, OUTPUT_MAN); scut; scut = find_public(scut, doc, "enumeration", NULL, OUTPUT_MAN))
    {
      name        = mxmlElementGetAttr(scut, "name");
      description = mxmlFindElement(scut, scut, "description", NULL, NULL, MXML_DESCEND_FIRST);
      printf(".SS %s\n", name);

      write_description(stdout, OUTPUT_MAN, description, NULL, 1);
      write_description(stdout, OUTPUT_MAN, description, NULL, 0);

      for (arg = mxmlFindElement(scut, scut, "constant", NULL, NULL, MXML_DESCEND_FIRST); arg; arg = mxmlFindElement(arg, scut, "constant", NULL, NULL, MXML_NO_DESCEND))
      {
	description = mxmlFindElement(arg, arg, "description", NULL, NULL, MXML_DESCEND_FIRST);
	printf(".TP 5\n%s\n.br\n", mxmlElementGetAttr(arg, "name"));
	write_description(stdout, OUTPUT_MAN, description, NULL, 1);
      }
    }
  }

 /*
  * List of functions...
  */

  if (find_public(doc, doc, "function", NULL, OUTPUT_MAN))
  {
    puts(".SH FUNCTIONS");

    for (function = find_public(doc, doc, "function", NULL, OUTPUT_MAN); function; function = find_public(function, doc, "function", NULL, OUTPUT_MAN))
    {
      name        = mxmlElementGetAttr(function, "name");
      description = mxmlFindElement(function, function, "description", NULL, NULL, MXML_DESCEND_FIRST);
      printf(".SS %s\n", name);

      write_description(stdout, OUTPUT_MAN, description, NULL, 1);

      puts(".PP\n"
           ".nf");

      arg = mxmlFindElement(function, function, "returnvalue", NULL, NULL, MXML_DESCEND_FIRST);

      if (arg)
	write_element(stdout, doc, mxmlFindElement(arg, arg, "type", NULL, NULL, MXML_DESCEND_FIRST), OUTPUT_MAN);
      else
	fputs("void", stdout);

      printf(" %s ", name);
      for (arg = mxmlFindElement(function, function, "argument", NULL, NULL, MXML_DESCEND_FIRST), prefix = '('; arg; arg = mxmlFindElement(arg, function, "argument", NULL, NULL, MXML_NO_DESCEND), prefix = ',')
      {
        type = mxmlFindElement(arg, arg, "type", NULL, NULL, MXML_DESCEND_FIRST);

	printf("%c\n    ", prefix);
	if (mxmlGetFirstChild(type))
	  write_element(stdout, doc, type, OUTPUT_MAN);
	fputs(mxmlElementGetAttr(arg, "name"), stdout);
        if ((defval = mxmlElementGetAttr(arg, "default")) != NULL)
	  printf(" %s", defval);
      }

      if (prefix == '(')
	puts("(void);");
      else
	puts("\n);");

      puts(".fi");

      write_description(stdout, OUTPUT_MAN, description, NULL, 0);
    }
  }

 /*
  * List of structures...
  */

  if (find_public(doc, doc, "struct", NULL, OUTPUT_MAN))
  {
    puts(".SH STRUCTURES");

    for (scut = find_public(doc, doc, "struct", NULL, OUTPUT_MAN); scut; scut = find_public(scut, doc, "struct", NULL, OUTPUT_MAN))
    {
      cname       = mxmlElementGetAttr(scut, "name");
      description = mxmlFindElement(scut, scut, "description", NULL, NULL, MXML_DESCEND_FIRST);
      printf(".SS %s\n", cname);

      write_description(stdout, OUTPUT_MAN, description, NULL, 1);

      printf(".PP\n"
             ".nf\n"
	     "struct %s\n{\n", cname);
      for (arg = mxmlFindElement(scut, scut, "variable", NULL, NULL, MXML_DESCEND_FIRST); arg; arg = mxmlFindElement(arg, scut, "variable", NULL, NULL, MXML_NO_DESCEND))
      {
	printf("  ");
	write_element(stdout, doc, mxmlFindElement(arg, arg, "type", NULL, NULL, MXML_DESCEND_FIRST), OUTPUT_MAN);
	printf("%s;\n", mxmlElementGetAttr(arg, "name"));
      }

      for (function = mxmlFindElement(scut, scut, "function", NULL, NULL, MXML_DESCEND_FIRST); function; function = mxmlFindElement(function, scut, "function", NULL, NULL, MXML_NO_DESCEND))
      {
        name = mxmlElementGetAttr(function, "name");

        printf("  ");

	arg = mxmlFindElement(function, function, "returnvalue", NULL, NULL, MXML_DESCEND_FIRST);

	if (arg)
	  write_element(stdout, doc, mxmlFindElement(arg, arg, "type", NULL, NULL, MXML_DESCEND_FIRST), OUTPUT_MAN);
	else if (strcmp(cname, name) && strcmp(cname, name + 1))
	  fputs("void ", stdout);

	fputs(name, stdout);

	for (arg = mxmlFindElement(function, function, "argument", NULL, NULL, MXML_DESCEND_FIRST), prefix = '('; arg; arg = mxmlFindElement(arg, function, "argument", NULL, NULL, MXML_NO_DESCEND), prefix = ',')
	{
	  type = mxmlFindElement(arg, arg, "type", NULL, NULL, MXML_DESCEND_FIRST);

	  putchar(prefix);
	  if (prefix == ',')
	    putchar(' ');

	  if (mxmlGetFirstChild(type))
	    write_element(stdout, doc, type, OUTPUT_MAN);
	  fputs(mxmlElementGetAttr(arg, "name"), stdout);
          if ((defval = mxmlElementGetAttr(arg, "default")) != NULL)
	    printf(" %s", defval);
	}

	if (prefix == '(')
	  puts("(void);");
	else
	  puts(");");
      }

      puts("};\n"
           ".fi");

      write_description(stdout, OUTPUT_MAN, description, NULL, 0);
    }
  }

 /*
  * List of types...
  */

  if (find_public(doc, doc, "typedef", NULL, OUTPUT_MAN))
  {
    puts(".SH TYPES");

    for (scut = find_public(doc, doc, "typedef", NULL, OUTPUT_MAN); scut; scut = find_public(scut, doc, "typedef", NULL, OUTPUT_MAN))
    {
      name        = mxmlElementGetAttr(scut, "name");
      description = mxmlFindElement(scut, scut, "description", NULL, NULL, MXML_DESCEND_FIRST);
      printf(".SS %s\n", name);

      write_description(stdout, OUTPUT_MAN, description, NULL, 1);

      fputs(".PP\n"
            ".nf\n"
	    "typedef ", stdout);

      type = mxmlFindElement(scut, scut, "type", NULL, NULL, MXML_DESCEND_FIRST);

      for (type = mxmlGetFirstChild(type); type; type = mxmlGetNextSibling(type))
      {
        string = mxmlGetText(type, &whitespace);

        if (!strcmp(string, "("))
	  break;
	else
	{
	  if (whitespace)
	    putchar(' ');

          write_string(stdout, string, OUTPUT_MAN, 0);
        }
      }

      if (type)
      {
       /*
        * Output function type...
	*/

        printf(" (*%s", name);

	for (type = mxmlGetNextSibling(mxmlGetNextSibling(type)); type; type = mxmlGetNextSibling(type))
	{
	  string = mxmlGetText(type, &whitespace);

	  if (whitespace)
	    putchar(' ');

          write_string(stdout, string, OUTPUT_MAN, 0);
        }

        puts(";");
      }
      else
	printf(" %s;\n", name);

      puts(".fi");

      write_description(stdout, OUTPUT_MAN, description, NULL, 0);
    }
  }

 /*
  * List of unions...
  */

  if (find_public(doc, doc, "union", NULL, OUTPUT_MAN))
  {
    puts(".SH UNIONS");

    for (scut = find_public(doc, doc, "union", NULL, OUTPUT_MAN); scut; scut = find_public(scut, doc, "union", NULL, OUTPUT_MAN))
    {
      name        = mxmlElementGetAttr(scut, "name");
      description = mxmlFindElement(scut, scut, "description", NULL, NULL, MXML_DESCEND_FIRST);
      printf(".SS %s\n", name);

      write_description(stdout, OUTPUT_MAN, description, NULL, 1);

      printf(".PP\n"
             ".nf\n"
	     "union %s\n{\n", name);
      for (arg = mxmlFindElement(scut, scut, "variable", NULL, NULL, MXML_DESCEND_FIRST); arg; arg = mxmlFindElement(arg, scut, "variable", NULL, NULL, MXML_NO_DESCEND))
      {
	printf("  ");
	write_element(stdout, doc, mxmlFindElement(arg, arg, "type", NULL, NULL, MXML_DESCEND_FIRST), OUTPUT_MAN);
	printf("%s;\n", mxmlElementGetAttr(arg, "name"));
      }

      puts("};\n"
           ".fi");

      write_description(stdout, OUTPUT_MAN, description, NULL, 0);
    }
  }

 /*
  * Variables...
  */

  if (find_public(doc, doc, "variable", NULL, OUTPUT_MAN))
  {
    puts(".SH VARIABLES");

    for (arg = find_public(doc, doc, "variable", NULL, OUTPUT_MAN); arg; arg = find_public(arg, doc, "variable", NULL, OUTPUT_MAN))
    {
      name        = mxmlElementGetAttr(arg, "name");
      description = mxmlFindElement(arg, arg, "description", NULL, NULL, MXML_DESCEND_FIRST);
      printf(".SS %s\n", name);

      write_description(stdout, OUTPUT_MAN, description, NULL, 1);

      puts(".PP\n"
           ".nf");

      write_element(stdout, doc, mxmlFindElement(arg, arg, "type", NULL, NULL, MXML_DESCEND_FIRST), OUTPUT_MAN);
      fputs(mxmlElementGetAttr(arg, "name"), stdout);
      if ((defval = mxmlElementGetAttr(arg, "default")) != NULL)
	printf(" %s", defval);
      puts(";\n"
           ".fi");

      write_description(stdout, OUTPUT_MAN, description, NULL, 0);
    }
  }

  if (footerfile)
  {
   /*
    * Use custom footer...
    */

    write_file(stdout, footerfile, OUTPUT_MAN);
  }
  else
  {
   /*
    * Use standard footer...
    */

    puts(".SH AUTHOR");
    puts(".PP");
    puts(author);

    puts(".SH COPYRIGHT");
    puts(".PP");
    puts(copyright);
  }
}


/*
 * 'write_scu()' - Write a structure, class, or union.
 */

static void
write_scu(FILE        *out,	/* I - Output file */
          int         mode,	/* I - Output mode */
          mxml_node_t *doc,	/* I - Document */
          mxml_node_t *scut)	/* I - Structure, class, or union */
{
  int		i;			/* Looping var */
  mxml_node_t	*function,		/* Current function */
		*arg,			/* Current argument */
		*description,		/* Description of function/var */
		*type;			/* Type for argument */
  const char	*name,			/* Name of function/type */
		*cname,			/* Class name */
		*defval,		/* Default value */
		*parent,		/* Parent class */
		*scope;			/* Scope for variable/function */
  int		inscope,		/* Variable/method scope */
		maxscope;		/* Maximum scope */
  char		prefix;			/* Prefix character */
  const char	*br = mode == OUTPUT_EPUB ? "<br />" : "<br>";
					/* Break sequence */
  static const char * const scopes[] =	/* Scope strings */
		{
		  "private",
		  "protected",
		  "public"
		};


  cname       = mxmlElementGetAttr(scut, "name");
  description = mxmlFindElement(scut, scut, "description", NULL, NULL, MXML_DESCEND_FIRST);

  fprintf(out, "<h3 class=\"%s\">%s<a id=\"%s\">%s</a></h3>\n", mxmlGetElement(scut), get_comment_info(description), cname, cname);

  if (description)
    write_description(out, mode, description, "p", 1);

  fprintf(out, "<p class=\"code\">%s %s", mxmlGetElement(scut), cname);
  if ((parent = mxmlElementGetAttr(scut, "parent")) != NULL)
    fprintf(out, " %s", parent);
  fprintf(out, " {%s\n", br);

  maxscope = !strcmp(mxmlGetElement(scut), "class") ? 3 : 1;

  for (i = 0; i < maxscope; i ++)
  {
    inscope = maxscope == 1;

    for (arg = mxmlFindElement(scut, scut, "variable", NULL, NULL, MXML_DESCEND_FIRST); arg; arg = mxmlFindElement(arg, scut, "variable", NULL, NULL, MXML_NO_DESCEND))
    {
      if (maxscope > 1 && ((scope = mxmlElementGetAttr(arg, "scope")) == NULL || strcmp(scope, scopes[i])))
	continue;

      if (!inscope)
      {
	inscope = 1;
	fprintf(out, "&#160;&#160;%s:%s\n", scopes[i], br);
      }

      fputs("&#160;&#160;&#160;&#160;", out);
      write_element(out, doc, mxmlFindElement(arg, arg, "type", NULL, NULL, MXML_DESCEND_FIRST), OUTPUT_HTML);
      fprintf(out, "%s;%s\n", mxmlElementGetAttr(arg, "name"), br);
    }

    for (function = mxmlFindElement(scut, scut, "function", NULL, NULL, MXML_DESCEND_FIRST); function; function = mxmlFindElement(function, scut, "function", NULL, NULL, MXML_NO_DESCEND))
    {
      if (maxscope > 1 && ((scope = mxmlElementGetAttr(arg, "scope")) == NULL || strcmp(scope, scopes[i])))
	continue;

      if (!inscope)
      {
	inscope = 1;
        fprintf(out, "&#160;&#160;%s:%s\n", scopes[i], br);
      }

      name = mxmlElementGetAttr(function, "name");

      fputs("&#160;&#160;&#160;&#160;", out);

      arg = mxmlFindElement(function, function, "returnvalue", NULL, NULL, MXML_DESCEND_FIRST);

      if (arg)
	write_element(out, doc, mxmlFindElement(arg, arg, "type", NULL, NULL, MXML_DESCEND_FIRST), OUTPUT_HTML);
      else if (strcmp(cname, name) && strcmp(cname, name + 1))
	fputs("void ", out);

      fprintf(out, "<a href=\"#%s.%s\">%s</a>", cname, name, name);

      for (arg = mxmlFindElement(function, function, "argument", NULL, NULL, MXML_DESCEND_FIRST), prefix = '('; arg; arg = mxmlFindElement(arg, function, "argument", NULL, NULL, MXML_NO_DESCEND), prefix = ',')
      {
	type = mxmlFindElement(arg, arg, "type", NULL, NULL, MXML_DESCEND_FIRST);

	putc(prefix, out);
	if (prefix == ',')
	  putc(' ', out);

	if (mxmlGetFirstChild(type))
	  write_element(out, doc, type, OUTPUT_HTML);

	fputs(mxmlElementGetAttr(arg, "name"), out);
	if ((defval = mxmlElementGetAttr(arg, "default")) != NULL)
	  fprintf(out, " %s", defval);
      }

      if (prefix == '(')
	fprintf(out, "(void);%s\n", br);
      else
	fprintf(out, ");%s\n", br);
    }
  }

  fputs("};</p>\n"
	"<h4 class=\"members\">Members</h4>\n"
	"<table class=\"list\"><tbody>\n", out);

  for (arg = mxmlFindElement(scut, scut, "variable", NULL, NULL, MXML_DESCEND_FIRST); arg; arg = mxmlFindElement(arg, scut, "variable", NULL, NULL, MXML_NO_DESCEND))
  {
    description = mxmlFindElement(arg, arg, "description", NULL, NULL, MXML_DESCEND_FIRST);

    fprintf(out, "<tr><th>%s %s</th>\n", mxmlElementGetAttr(arg, "name"), get_comment_info(description));

    write_description(out, mode, description, "td", -1);
    fputs("</tr>\n", out);
  }

  fputs("</tbody></table>\n", out);

  for (function = mxmlFindElement(scut, scut, "function", NULL, NULL, MXML_DESCEND_FIRST); function; function = mxmlFindElement(function, scut, "function", NULL, NULL, MXML_NO_DESCEND))
  {
    write_function(out, mode, doc, function, 4);
  }
}


/*
 * 'write_string()' - Write a string, quoting HTML special chars as needed.
 */

static void
write_string(FILE       *out,		/* I - Output file */
             const char *s,		/* I - String to write */
             int        mode,		/* I - Output mode */
             int        len)		/* I - Length or offset */
{
  const char	*start = s;		/* Start of string */
  const char	*end;			/* End of string */


  if (!s)
    return;

  if (len <= 0)
    end = s + strlen(s) - len;
  else
    end = s + len;

  switch (mode)
  {
    case OUTPUT_EPUB :
    case OUTPUT_HTML :
    case OUTPUT_XML :
        while (*s && s < end)
        {
          if (*s == '&')
            fputs("&amp;", out);
          else if (*s == '<')
            fputs("&lt;", out);
          else if (*s == '>')
            fputs("&gt;", out);
          else if (*s == '\"')
            fputs("&quot;", out);
          else if (!strncasecmp(s, COPYRIGHT_ASCII, COPYRIGHT_ASCII_LEN) && (s == start || isspace(s[-1] & 255)) && (!s[COPYRIGHT_ASCII_LEN] || isspace(s[COPYRIGHT_ASCII_LEN] & 255)))
          {
            fputs(COPYRIGHT_UTF8, out);
            s += COPYRIGHT_ASCII_LEN - 1;
          }
          else if (!strncasecmp(s, REGISTERED_ASCII, REGISTERED_ASCII_LEN) && (s == start || isspace(s[-1] & 255)) && (!s[REGISTERED_ASCII_LEN] || isspace(s[REGISTERED_ASCII_LEN] & 255)))
          {
            fputs(REGISTERED_UTF8, out);
            s += REGISTERED_ASCII_LEN - 1;
          }
          else if (!strncasecmp(s, TRADEMARK_ASCII, TRADEMARK_ASCII_LEN) && (s == start || isspace(s[-1] & 255)) && (!s[TRADEMARK_ASCII_LEN] || isspace(s[TRADEMARK_ASCII_LEN] & 255)))
          {
	    fputs(TRADEMARK_UTF8, out);
            s += TRADEMARK_ASCII_LEN - 1;
          }
          else
            putc(*s, out);

          s ++;
        }
        break;

    case OUTPUT_MAN :
        if (*s == '\'' || *s == '.')
	  putc('\\', out);		// Escape leading "." or "'"

        while (*s && s < end)
        {
          if (!strncasecmp(s, COPYRIGHT_ASCII, COPYRIGHT_ASCII_LEN) && (s == start || isspace(s[-1] & 255)) && (!s[COPYRIGHT_ASCII_LEN] || isspace(s[COPYRIGHT_ASCII_LEN] & 255)))
          {
            fputs("\\[co]", out);
            s += COPYRIGHT_ASCII_LEN;
          }
          else if (!strncmp(s, COPYRIGHT_UTF8, COPYRIGHT_UTF8_LEN))
          {
            fputs("\\[co]", out);
            s += COPYRIGHT_UTF8_LEN;
          }
          else if (!strncasecmp(s, REGISTERED_ASCII, REGISTERED_ASCII_LEN) && (s == start || isspace(s[-1] & 255)) && (!s[REGISTERED_ASCII_LEN] || isspace(s[REGISTERED_ASCII_LEN] & 255)))
          {
            fputs("\\*R", out);
            s += REGISTERED_ASCII_LEN;
          }
          else if (!strncmp(s, REGISTERED_UTF8, REGISTERED_UTF8_LEN))
          {
            fputs("\\*R", out);
            s += REGISTERED_UTF8_LEN;
          }
          else if (!strncasecmp(s, TRADEMARK_ASCII, TRADEMARK_ASCII_LEN) && (s == start || isspace(s[-1] & 255)) && (!s[TRADEMARK_ASCII_LEN] || isspace(s[TRADEMARK_ASCII_LEN] & 255)))
          {
	    fputs("\\*(Tm", out);
            s += TRADEMARK_ASCII_LEN;
          }
          else if (!strncmp(s, TRADEMARK_UTF8, TRADEMARK_UTF8_LEN))
          {
            fputs("\\*(Tm", out);
            s += TRADEMARK_UTF8_LEN;
          }
          else
          {
	    if (*s == '\\' || *s == '-')
	      putc('\\', out);

	    putc(*s++, out);
	  }
        }
        break;
  }
}


/*
 * 'ws_cb()' - Whitespace callback for saving.
 */

static const char *			/* O - Whitespace string or NULL for none */
ws_cb(mxml_node_t *node,		/* I - Element node */
      int         where)		/* I - Where value */
{
  const char *name;			/* Name of element */
  int	depth;				/* Depth of node */
  static const char *spaces = "                                        ";
					/* Whitespace (40 spaces) for indent */


  name = mxmlGetElement(node);

  switch (where)
  {
    case MXML_WS_BEFORE_CLOSE :
        if (strcmp(name, "argument") && strcmp(name, "class") && strcmp(name, "constant") && strcmp(name, "enumeration") && strcmp(name, "function") && strcmp(name, "codedoc") && strcmp(name, "namespace") && strcmp(name, "returnvalue") && strcmp(name, "struct") && strcmp(name, "typedef") && strcmp(name, "union") && strcmp(name, "variable"))
	  return (NULL);

	for (depth = -4; node; node = mxmlGetParent(node), depth += 2)
	  ;				// Determine depth

	if (depth > 40)
	  return (spaces);
	else if (depth < 2)
	  return (NULL);
	else
	  return (spaces + 40 - depth);

    case MXML_WS_AFTER_CLOSE :
	return ("\n");

    case MXML_WS_BEFORE_OPEN :
	for (depth = -4; node; node = mxmlGetParent(node), depth += 2)
	  ;				// Determine depth

	if (depth > 40)
	  return (spaces);
	else if (depth < 2)
	  return (NULL);
	else
	  return (spaces + 40 - depth);

    default :
    case MXML_WS_AFTER_OPEN :
        if (strcmp(name, "argument") && strcmp(name, "class") && strcmp(name, "constant") && strcmp(name, "enumeration") && strcmp(name, "function") && strcmp(name, "codedoc") && strcmp(name, "namespace") && strcmp(name, "returnvalue") && strcmp(name, "struct") && strcmp(name, "typedef") && strcmp(name, "union") && strcmp(name, "variable") && strncmp(name, "?xml", 4))
	  return (NULL);
	else
          return ("\n");
  }
}
