# Munich Cinema Display

A Heltec WiFi LoRa 32 V3 that shows what is playing in Munich cinemas today, with start times.

**Modes** (hold the PRG button to switch):

| Mode | Shows |
|---|---|
| KINO | one cinema at a time: its films and times |
| FILM | one film at a time: every cinema showing it, with times |
| GLEICH | everything starting in the next 90 minutes, city-wide |

**Button:** tap = next · double tap = previous · hold (0.7 s) = next mode.
Long pages scroll by themselves. Shows that already started disappear; shows starting within 45 min get a `*`.
`OV` / `OmU` / `3D` versions are listed on their own line. After the last show, the display switches to tomorrow.
Screen turns off 01:00–07:00 (any press wakes it for 2 min) and dims after 5 min without a press (OLED care).

## How it works

```
Kinoheld (public GraphQL, same data as kinoheld.de)
      |  twice a day, GitHub Actions runs scraper/scrape.py
      v
data/cinemas.txt   (about 6 KB, in your GitHub repo)
      |  board downloads it every 3 h (WiFi on only for the download)
      v
Heltec board  --  keeps the last copy in flash, so it still works after a power cut / without WiFi
```

If Kinoheld changes something, only the scraper needs fixing; the board does not have to be re-flashed.
The scraper refuses to overwrite good data when a run returns suspiciously few shows.

## Setup (about 20 minutes)

1. **Try the scraper on your PC**
   ```
   pip install tzdata
   python scraper\scrape.py
   ```
   It should print something like `ok: 276 shows, 25 cinemas, 66 films`. Look at `data\cinemas.txt`.
   (Only Munich city cinemas are kept. Add `--suburbs` for Haar, Gauting, Neufahrn... in the workflow if you want them.)
2. **GitHub:** create a *public* repository (e.g. `munich-cinema-display`), upload this folder, then open the
   *Actions* tab, enable workflows, pick "Scrape Munich cinema program" and press *Run workflow*.
   After a minute `data/cinemas.txt` is in the repo.
3. **Board config:** copy `board/src/secrets.h.example` to `board/src/secrets.h` and fill in the friend's WiFi,
   your GitHub user name in `DATA_URL`, and (optionally) his favourite cinema in `START_CINEMA`.
4. **Flash:** in the `board` folder run `python -m platformio run -t upload`
5. Unplug/replug; first start takes ~20 s (WiFi + download). If it says "Keine Daten", check the URL in a browser.

## Things worth knowing

- **Unofficial data source.** Kinoheld's endpoint is not a documented public API. It works today; it could change.
  The scraper only makes ~3 requests per run. It is meant for private use.
- **Showtimes can change.** The data is as fresh as the last scrape (twice a day). Always check the cinema's own
  site before travelling far.
- **GitHub schedules** can run a few minutes late, and GitHub switches scheduled workflows off after 60 days without
  any repository activity. If the display shows `!` in the header (data older than 26 h), open the Actions tab and
  re-enable the workflow.
- **Battery:** the 1100 mAh cell is a backup; for a shelf display use USB power.
- Titles are converted to the characters the OLED font can draw (umlauts and ß work; exotic punctuation is simplified).

## Tests (no board or network needed)

`cd test && ./run_tests.sh` - scraper against a mock API response built from real rows, screen logic, and the button gestures.
