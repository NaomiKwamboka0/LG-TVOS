FAMILY TV FOR WINDOWS
=====================

Start:   double-click FamilyTV.exe
         The first time, Windows may say "Windows protected your PC".
         Click "More info", then "Run anyway". (Family TV is not signed
         by a company, so Windows does not know it yet.)

Keys:    Arrows      move around / channel up and down / volume
         Enter       OK (choose; while watching: the channel list)
         Backspace   Back (while watching: back to the home screen)
         F11 or F    full screen on and off
         Q           close Family TV

Live channels play from each station's YouTube live stream, so they need
internet. A channel can take 10-15 seconds to start. Going back to the
home screen stops the stream, so no data is used while you are there.

Data use per hour of watching (Parent settings, "Live TV picture"):
         360p  about 0.5 GB      480p  about 0.6 GB (default)
         720p  about 1.4 GB

Changing channels: open channels.txt (next to FamilyTV.exe) in Notepad.
Each line is   number|name|address|#colour   - see the notes at the top
of the file. Use a channel's ID address (youtube.com/channel/UC.../live).

If a channel says "The channel could not play" for every channel, YouTube
may have changed something. Update yt-dlp: open the FamilyTV folder,
click the address bar, type   cmd   and press Enter, then type
         yt-dlp.exe -U
and press Enter. Restart Family TV.

If something goes wrong, the file familytv.log in
         %APPDATA%\FamilyTV\FamilyTV
says what happened (paste that line into the Explorer address bar).
Your settings, PIN and game saves are kept in the same folder.
