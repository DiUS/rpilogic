rpilogic
========

Turn a Rasperry Pi into a simple offline logic analyser.

"rpilogic" is little more than a hack, but a very useful one at that. It
enables simultaneous capture of the first 32 GPIO pins at relatively
high speeds - we have tested it up to 2.5MHz in order to analyse a 1MHz
SPI link.

Internally the application uses an appropriately sized cyclic buffer into
which it continuously stores the GPIO state. To achieve best timing and
support high speeds, it uses busywaits, and as such this program is best
run on a dedicated pi.

The capture runs until the program is told to exit, either when it's told
to quit (SIGINT, SIGTERM, SIGQUIT), or when a specified GPIO line is high.
The latter allows for the use case where the monitored device has been
instrumented to itself be able to signal that the condition of interest
has occurred. When analysing aforementioned SPI link, we used this trigger
whenever we noticed we'd received something strange on the SPI link, which
meant we could leave everything running unsupervised.

When the capture is stopped, the entire buffer is written out to the
specified file. For a large buffer, this can take quite a while.

Once the capture file has been obtained, the helper program "rpldecode"
is used to turn this binary dump into ASCII file(s) containing all the
transitions. These files can be used e.g. by "gnuplot" (or even Excel)
to graph it all up. We like gnuplot, as it allows for interactive
analysis of the entire dataset. In fact, we like it so much that
"rpldecode" prints the basic 'plot' command to use in gnuplot, so that
it's a simple copy'n'paste to get a pretty picture up on the screen.


### rpilogic syntax

  `rpilogic -o <filename> [-t <stop-trigger-gpio>] [-s <buf-size-in-sec>] [-h <hz>]`

The destination filename is mandatory, everything else is optional.

If a **-t** is given, that GPIO pin will be monitored for a "high", and when
seen will cause rpilogic to stop the capture, write the capture file and
then exit. Only one trigger pin is currently supported. If no trigger is
specified, the capture only ends when the program is told to quit.

The  **-s** is used to size the capture buffer, so that the specified
number of seconds worth of data is kept in the cyclic buffer. The default
value if 60.

The **-h** option controls the sampling frequency used. It defaults to
500kHz, but supports as low as 1Hz. Higher frequence and longer capture
window obviously increases the size of the capture buffer, and hence
the memory requirements. If the buffer can not be allocated at startup,
an error will be printed.

Example:
  sudo rpilogic -o gpio.log -s 40 -h 2500000

### rpldecode syntax

  `rpldecode  -f <file> [-g <offset_for_gnuplot>] [-o <combined_output_file>] <gpio:name...>`

The output filename is mandatory, as is at least one GPIO definition. These
definitions simply map a GPIO pin number to a logical name. Unless the -o
option is used, this name specifies the output file name for this channel.

If **-g** if given, the values output will be offset by the specified amount,
meaning that when plotted they'll show up above each other, as one would
expect in a logic analyser, rather than plotted right on top of each other.

The **-o** option can be used create a single, multi-column file rather than
the default which is one file per GPIO. Each column is named per the GPIO
definition.

Example:
  rpldecode -f gpio.log -g 2 8:csel 7:miso 25:mosi 23:sck


### Other software
"rpilogic" uses the nifty "bcm2835" library for its fast GPIO access:
  http://www.airspayce.com/mikem/bcm2835/index.html

We also suggest the use of "gnuplot" to view the captured data with:
  sudo apt-get install gnuplot-x11
  http://www.gnuplot.info/

