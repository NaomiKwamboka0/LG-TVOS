# Privacy and data protection

*Family TV, version 1.0. Last updated 30 October 2026.*

**In short: Family TV has no accounts, no analytics and no server of its own. Everything it remembers stays on your computer, and nothing about you is sent to its author.**

## Who is responsible

Family TV is free, open-source software written by Naomi Kwamboka Onyancha ("the author"). The author runs no service behind it and receives no data from it. Because the program never sends your data to the author, the author does not collect, store or process your personal data. You, on your own computer, are in control of it.

Questions or a privacy concern: open an issue at https://github.com/NaomiKwamboka0/family-tv/issues.

## What stays on your computer

Family TV saves these files in its own folder and nowhere else:

| What | Why | Where |
|---|---|---|
| Settings: language, picture quality, play-time limit, which games a child may play | To remember your choices | `settings.sav` |
| The parent PIN, stored only as a salted, one-way hash (the PIN itself is never saved) | To lock Parent settings | `settings.sav` |
| Game progress: high scores, saved matches, your garden | So games continue | `racer.sav`, `penalty.sav`, `garden1.sav` |
| The last channel watched, volume and full-screen choice | So the app opens as you left it | `player.sav` |
| A log of recent playback messages, which can include the addresses of streams and videos you opened | To diagnose problems | `familytv.log` |
| Your channel list | Your own edits | `channels.txt` |

**Where the folder is:** `%APPDATA%\FamilyTV\FamilyTV` on Windows, `~/Library/Application Support/FamilyTV/FamilyTV` on a Mac.

**Your control:** you can read these files, copy them, or delete the folder at any time to erase everything Family TV knows. Uninstalling Family TV keeps the folder so your family's settings survive an update; delete it by hand to remove them.

The play-time counter for a child profile is kept on the same computer and is never reported anywhere.

## What goes over the internet

Family TV itself has no tracking. It connects to other companies' services only when you use them, to do what you asked:

- **Live channels:** Family TV asks each station's YouTube live stream for its video and plays it. YouTube (Google) sees your internet address and the stream you watch, as it would in any browser.
- **Sports tab:** it asks YouTube which sports channels are live, and searches YouTube's live results for football.
- **YouTube, Netflix and Google tiles:** these show the real websites inside the Family TV window using your computer's built-in web view. They are those companies' own services. If you sign in, they store cookies and your sign-in in the web view on your computer, as a browser would, and their own privacy policies and terms apply: <https://policies.google.com/privacy> (Google and YouTube) and <https://help.netflix.com/legal/privacy> (Netflix). Family TV does not read, copy or send your passwords or what you watch there.
- **Updates:** Family TV does not check for updates or phone home. New versions come only when you download them from GitHub.

When you download Family TV from GitHub, GitHub's own privacy statement applies to that download.

## Children

Family TV is meant for families to use together. The child profile has no sign-ins, no web access, no search and no way to contact anyone: YouTube, Netflix, Google and unvetted football streams are available to the parent profile only, and a parent's PIN is needed for settings. The author collects no information from children, or anyone, because the app sends none to the author.

## Security

The PIN is protected with a salted one-way hash, and save files carry checksums so damaged files are detected rather than trusted. Installers are not yet signed by a company, so your computer may warn you the first time. Check the downloaded file against `SHA256SUMS.txt` on the release page if you want to be sure it is untouched.

## Your rights

Because the author holds none of your data, there is nothing for the author to give you, correct or delete: you already hold all of it, in the folder above. If your law gives you rights over personal data (for example Kenya's Data Protection Act, 2019, or the EU GDPR), you exercise them with the companies whose services you sign in to (Google/YouTube, Netflix, and GitHub), not with Family TV.

## Changes

If a future version changes what data Family TV handles, this page will change with it, and the "last updated" date above will move. The history of every change is public in this repository.

*This is a plain-language notice from the author, not legal advice. Anyone distributing or running Family TV commercially should take their own legal advice.*
