Chess GUI Arena for Linux
-------------------------

http://www.playwitharena.com

Version History:
-----------------------------------------

Arena 3.9beta for Linux, 2019-12-28
  * GUI capable of using also Syzygy tablebases (up to 7 men)
   * Please note that Syzygy tablebases show "distance to zero(ing the 50-move counter)" values, not "distance to mate"
  * Tablebases handling improved: Syzygy, Gaviota and Scorpio tablebases now also get common settings (like Nalimov), so you do not have to set it for each engine separately
  * Dialogs a bit larger for Linuxes that use much space for some dialogue elemnts
  * Export or save selected games from PGN now inserts line feeds if necessary (sometimes this was wrong)
  * Book import faster and auto-closing PGN list for speed
  * Using local thousand separator of operating system now everywhere (e.g.: ,.' or others)
  * Small improvements: Tab-order within dialogs, avoiding stay-on-top windows
  * Ordo updated to 1.2.6
  * Large symbols for the toolbar and other symbol set available
  * Recently used engines now directly selectable in menu
  * Help improved
  * Move list faster and improved look
  * Added 4 buttons under move list to show/hide values, comments, principal variations and book names
  * Added no. of engine cores selectable in main menu, with keyboard shortcuts
  * Commands [%clk, [%eval and [%emt in PGN files supported for reading
  * Speed of arrows improved, hatched squares improved, arrow transparency now changeable
  * Filter position: fixed more pieces than one on a single square, which were distorted before
  * Fixed Tablebase move list, was not always aligned
  * Even better support for dark OS themes
  * Temp tab look improved
  * Variation board: Mouse wheel supported for browsing the moves
  * Character encoding problems wich special characters in some PGN games solved 
  * Compatible graphics mode for simple or not fully functioning graphics drivers (so we can see the pieces, e.g. Raspbian on Raspi 4)
  * Lot of bug fixes and small improvements

TODO (missing right now) for next main (non-beta) version (these are the reasons why this is labled as beta):
  * Testing tournamets and DGT necessary
  * ARM 32 bit now also can access Gaviota tablebaes
  * ARM 64 bit version added
  * More engines for ARM-versions added
  * Help improved


Arena 1.1 for Linux, 2017-01-29
  * Tournament bugs removed
  * Mousewheel deactivated during tournaments
  * Open tournament in browser from tournament dialogue now works
  * Free RAM display now from /proc/meminfo instead of SysInfo
  * Rating calculation: "Aggregate results" and "copy files to" now works
  * Rating calculation: Ordo works now additionally in ELOStat mode
  * Rating calculation: You can import the rating results from file into Arena
  * Many other bugs removed
  * Better support for dark themes
  * Small improvements in various areas

Arena 1.0 for Linux, 2016-11-12
  * First public release of the Linux version
  * Differences to the Windows version
    * Better integration into your Linux desktop than the Wine version
    * Can run all engines types: Linux, and Windows (if Wine installed)
    * Completely UTF-8
    * Uses Ordo for rating calculation
    * no Citrine, Autoplayer, ICS or printing yet
  * Sounds via external command line player (mpg123, music123 or music321) possible

If you would like to create a translation, contact the Arena team
via the support form on the website.


