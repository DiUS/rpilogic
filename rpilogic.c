/**********************************************************************

  Copyright 2014 DiUS Computing Pty. Ltd. All rights reserved.

  Licensed under GPLv2.

**********************************************************************/

#include <bcm2835.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>


typedef struct
{
  uint32_t nsize;
  uint32_t *base;
  uint32_t *head;
} buffer_t;


volatile int terminate = 0;


static void on_sig (int unused)
{
  (void)unused;
  terminate = 1;
  signal (SIGINT, SIG_DFL);
  signal (SIGTERM, SIG_DFL);
  signal (SIGQUIT, SIG_DFL);
}


// Block access to levels on gpio 0-31
static uint32_t gpio_bank_lev (void)
{
  volatile uint32_t *addr = bcm2835_gpio + BCM2835_GPLEV0/4;
  return *addr;
}


static char gpio_pin_func (uint8_t pin)
{
  uint8_t shift = (pin % 10) * 3;
  volatile uint32_t *addr = bcm2835_gpio + BCM2835_GPFSEL0/4 + (pin/10);
  uint32_t mode = (bcm2835_peri_read (addr) >> shift) & BCM2835_GPIO_FSEL_MASK;
  switch (mode)
  {
    case BCM2835_GPIO_FSEL_INPT: return 'i';
    case BCM2835_GPIO_FSEL_OUTP: return 'o';
    case BCM2835_GPIO_FSEL_ALT0: return '0';
    case BCM2835_GPIO_FSEL_ALT1: return '1';
    case BCM2835_GPIO_FSEL_ALT2: return '2';
    case BCM2835_GPIO_FSEL_ALT3: return '3';
    case BCM2835_GPIO_FSEL_ALT4: return '4';
    case BCM2835_GPIO_FSEL_ALT5: return '5';
    default: return '?';
  }
}


static void syntax (const char *name)
{
  fprintf (stderr, "Syntax: %s -o <filename> [-t <stop-trigger-gpio>] [-s <buf-size-in-sec> [-h <hz>]\n\n", name);
  exit (1);
}

static void ns_busy_delay (uint32_t ns)
{
  volatile uint64_t x = 0;
  uint64_t limit = 100ull * ns;
  while (x < limit)
    x += 1600; // roughly
}

static void buffer_write (buffer_t *buf, uint32_t val)
{
  *buf->head++ = val;
  if (buf->head >= (buf->base + buf->nsize))
    buf->head = buf->base;
}


int main (int argc, char *argv[])
{
  const char *outfname = NULL;
  int trigger_pin = -1;
  int seconds = 60;
  int hz = 500000;

  int opt;
  while ((opt = getopt (argc, argv, "h:o:t:s:")) != -1)
  {
    switch (opt)
    {
      case 'h': hz = atoi (optarg); break;
      case 'o': outfname = optarg; break;
      case 't': trigger_pin = atoi (optarg); break;
      case 's': seconds = atoi (optarg); break;
      default:
        syntax (argv[0]);
    }
  }
  if (!outfname || seconds <= 0 || hz <= 0)
    syntax (argv[0]);

  FILE *f = fopen (outfname, "w");
  if (!f)
  {
    perror (outfname);
    return 2;
  }

  if (!bcm2835_init ())
  {
    fprintf (stderr, "%s: failed to init bcm2835 lib\n", argv[0]);
    return 3;
  }

  signal (SIGINT, on_sig);
  signal (SIGTERM, on_sig);
  signal (SIGQUIT, on_sig);

  fprintf (stderr, "GPIO configuration:");
  uint8_t i = 0;
  for (; i < 32; ++i)
  {
    if ((i % 4) == 0)
      fputc (' ', stderr);
    fputc (gpio_pin_func (i), stderr);
  }
  fprintf (stderr, "\n");

  uint32_t nsize = seconds * hz;
  buffer_t buf = { nsize, calloc (sizeof (uint32_t), nsize), NULL };
  if (!buf.base)
  {
    perror ("malloc");
    return 4;
  }
  buf.head = buf.base;
  fprintf (stderr, "Allocated %u MB buffer\n",
    nsize * sizeof (uint32_t) / (1024*1024));

  uint32_t delay_ns = 1000000000lu / hz;
  fprintf (stderr, "Sampling rate: %d requested, %d effective\n",
    hz, (int)(1000000000llu / delay_ns));

  while (!terminate)
  {
    uint32_t val = gpio_bank_lev ();
    buffer_write (&buf, val);
    ns_busy_delay (delay_ns);
    if (trigger_pin != -1 && (val & (1 << trigger_pin)))
      break;
  }

  fprintf (stderr, "Dumping buffer to file: %s...\n", outfname);
  uint32_t sz1 = (buf.base + buf.nsize) - buf.head;
  uint32_t sz2 = buf.head - buf.base;
  if (fwrite (buf.head, sizeof(uint32_t), sz1, f) != sz1 ||
      fwrite (buf.base, sizeof (uint32_t), sz2, f) != sz2)
  {
    perror (outfname);
    return 5;
  }
  fclose (f);
  fprintf (stderr, "Done.\n");

  bcm2835_close ();

  return 0;
}
