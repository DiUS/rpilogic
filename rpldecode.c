/**********************************************************************

  Copyright 2014 DiUS Computing Pty. Ltd. All rights reserved.

  Licensed under GPLv2.

**********************************************************************/

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <stdbool.h>

static double offset=0;
static bool combined_file=false;

void syntax (const char *name)
{
  fprintf (stderr, "Syntax: %s -f <file> [-g <offset_for_gnuplot>] [-o <combined_output_file>] <gpio:name...>\n\n", name);
  exit (1);
}

static void print_point(FILE* f, int ind, int x, double val)
{
  char t[100];
  char out[100];

  if (!combined_file || ind==0)
    sprintf(t,"%u\t",x);
  else
    t[0]=0;

  sprintf (out,"%s%f",t, val+offset*ind);
  int len=strlen(out);
  while (out[len-1]=='0')
  {
    len--;
  }
  if (out[len-1]=='.')
    len--;
  out[len]=0;
  fprintf(f,"%s%c",out,combined_file?'\t':'\n');
}

static void print_end(FILE* f)
{
  if (combined_file)
    fprintf(f,"\n");
}

int main (int argc, char *argv[])
{
  const char *fname = NULL;
  const char *oname = NULL;
  int opt;

  while ((opt = getopt (argc, argv, "f:g:o:")) != -1)
  {
    switch (opt)
    {
      case 'f': fname = optarg; break;
      case 'o': oname = optarg; break;
      case 'g': offset=atof(optarg); break;
      default: syntax (argv[0]);
    }
  }

  int nselected = 0;
  struct {
    unsigned pin;
    const char *name;
    FILE *f;
    int lastval;
    uint32_t lastcount;
  } selected[32];

  FILE* of=NULL;
  if (oname)
  {
    of=fopen(oname,"w");
    if (!of)
    {
      perror(oname);
      return 4;
    }
    combined_file=true;
  }

  while (argc > optind)
  {
    --argc;
    const char *sel = argv[argc];
    const char *colon = strchr (sel, ':');
    if (!colon)
      syntax (sel);
    selected[nselected].pin = atoi (sel);
    selected[nselected].name = colon + 1;
    if (selected[nselected].pin > 31)
      syntax (argv[0]);
    selected[nselected].f = of?:fopen (selected[nselected].name, "w");
    if (!selected[nselected].f)
    {
      perror (selected[nselected].name);
      return 4;
    }
    selected[nselected].lastval=-1;
    selected[nselected].lastcount=0;
    ++nselected;
  }

  if (!fname)
    syntax (argv[0]);

  FILE *f = fopen (fname, "r");
  if (!f)
  {
    perror (fname);
    return 2;
  }

  if (of)
  {
    fprintf(of,"t\t");
    for (int i=0; i < nselected; ++i)
      fprintf(of,"%s\t",selected[i].name);
    fprintf(of,"\n");
  }

  for (int i=0; i < nselected; ++i)
    print_point(selected[i].f,i,0,-0.25);
  print_end(of);

  for (int i=0; i < nselected; ++i)
    print_point(selected[i].f,i,0,1.25);
  print_end(of);

  uint32_t count=0;
  while (!feof (f))
  {
    uint32_t val;
    if (fread (&val, sizeof (val), 1, f) == 1)
    {
      bool changed=false;
      bool last_needs_print=false;

      if (of)
      {
        for (int i=0; i < nselected; ++i)
        {
          int dval=val & (1 << selected[i].pin) ? 1 : 0;
          if (dval!=selected[i].lastval)
          {
            changed=true;
            if (selected[i].lastval!=-1 && selected[i].lastcount!=count-1)
              last_needs_print=true;
          }
        }
      }

      for (int i=0; i < nselected; ++i)
      {
        int dval=val & (1 << selected[i].pin) ? 1 : 0;
        if (last_needs_print ||
            (dval!=selected[i].lastval && count!=0 && selected[i].lastcount!=count-1))
        {
          print_point(selected[i].f,i,count-1,selected[i].lastval);
        }
      }
      if (last_needs_print)
        print_end(of);

      for (int i=0; i < nselected; ++i)
      {
        int dval=val & (1 << selected[i].pin) ? 1 : 0;
        if (changed || dval!=selected[i].lastval)
        {
          print_point(selected[i].f,i,count,dval);
          selected[i].lastval=dval;
          selected[i].lastcount=count;
        }
      }
      if (changed)
        print_end(of);
    }
    else if (!feof (f))
    {
      perror (fname);
      return 3;
    }
    count++;
  }

  // Extend to the end of the observations
  for (int i=0; i < nselected; ++i)
    print_point(selected[i].f,i,count-1,selected[i].lastval);
  print_end(of);

  if (of)
  {
    printf("plot ");
    for (int i=0; i < nselected; ++i)
      printf("%c\"%s\" using \"t\":\"%s\"",
             i?',':' ',
             oname,
             selected[i].name);
    printf("\n");
  }
  else
  {
    printf("plot ");
    for (int i=0; i < nselected; ++i)
      printf("%c\"%s\"",
             i?',':' ',
             selected[i].name);
    printf("\n");
  }
  return 0;
}
