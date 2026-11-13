Files put here are copied into the app (`/app0/assets` on the console) by
`build.sh`. (This README is left out.)

- `fp/` - Firepower's own graphics, battlefields, sounds and tables, from YOUR
  Firepower disk: `python3 tools/extract_assets.py /path/to/Firepower.adf`.
  Not in the git repository (`.gitignore`): you bring the disk, as the Amiga
  app needs your Kickstart.

To try new files without building again, copy `fp/` over FTP straight into the
installed app: `/data/homebrew/PPSA99908/assets/fp/` on the console.
