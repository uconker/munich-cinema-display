#!/usr/bin/env python3
"""Daily Munich cinema scraper -> data/cinemas.txt (compact text format for the Heltec board).

Source: Kinoheld's public GraphQL endpoint, the same one kinoheld.de/kino/muenchen calls.
It is UNOFFICIAL and may change. Be polite: this makes ~3 requests per run, twice a day.

Usage:
  python scrape.py                      # live fetch -> ../data/cinemas.txt
  python scrape.py --input mock.json    # offline test with a saved API response
  python scrape.py --suburbs            # also keep Haar, Gauting, Neufahrn, ...
"""
import argparse, json, os, re, sys, time, unicodedata, urllib.request
from datetime import datetime, timedelta
from zoneinfo import ZoneInfo

ENDPOINT = "https://next-live.kinoheld.de/graphql"
QUERY = """query($cinemaProximity: Proximity,$periods:[ShowPeriod!],$excludeExpired:Boolean,$first:Int,$page:Int){
programShows(cinemaProximity:$cinemaProximity,periods:$periods,excludeExpired:$excludeExpired,first:$first,page:$page){
data{id name beginning flags{name} cinema{id name city{name}} movie{id title duration contentRating{minimumAge}}}
paginatorInfo{hasMorePages currentPage}}}"""

# cinemas that are not (or duplicate) cinemas; matched case-insensitively on the raw Kinoheld name
EXCLUDE = {"neues maxim münchen (alt)", "kulturverein olympiadorf"}
MIN_SHOWS = 40          # refuse to overwrite good data with a suspiciously small result
FLAG_OV, FLAG_OMU, FLAG_3D = 1, 2, 4
try:
    TZ = ZoneInfo("Europe/Berlin")
except Exception:
    sys.exit("Timezone data missing. On Windows run:  pip install tzdata")

PUNCT = {"–": "-", "—": "-", "‒": "-", "‘": "'", "’": "'", "‚": ",", "“": '"', "”": '"', "„": '"',
         "…": "...", " ": " ", "&": "&"}

def latin1_clean(s: str) -> str:
    """The OLED font covers Latin-1 only (umlauts, ß ok). Replace what it can't draw."""
    s = "".join(PUNCT.get(c, c) for c in s)
    s = re.sub(r"^(sneak preview)\b.*$", "Sneak Preview", s, flags=re.I)
    s = unicodedata.normalize("NFC", s)
    out = []
    for c in s:
        try:
            c.encode("latin-1"); out.append(c)
        except UnicodeEncodeError:
            base = unicodedata.normalize("NFKD", c).encode("ascii", "ignore").decode()
            out.append(base or "?")
    return re.sub(r"\s+", " ", "".join(out)).strip()

ACRONYMS = {"TU", "ABC", "OV", "OMU", "USA", "TV", "DDR", "ARRI", "FBI", "CIA", "UK", "KZ", "II", "III", "IV"}
SMALL = {"und", "der", "die", "das", "den", "dem", "des", "im", "in", "vom", "von", "mit", "zu", "zum",
         "zur", "am", "an", "auf", "für", "ein", "eine", "of", "and", "the", "to", "a", "in", "on", "or"}

def _cap_word(w: str) -> str:
    wl = w.lower()
    return re.sub(r"(^|[-(\"/:])([a-zäöüß])", lambda m: m.group(1) + m.group(2).upper(), wl)

def smart_case(s: str) -> str:
    """ALL CAPS -> Title Case (acronyms stay upper, small words lower). Mixed-case input is untouched."""
    if not (s.isupper() and len(s) > 3):
        return s
    out = []
    for i, w in enumerate(s.split(" ")):
        core = re.sub(r"[^A-Za-zÄÖÜäöüß0-9]", "", w)
        if core in ACRONYMS or any(c.isdigit() for c in core):
            out.append(w)
        elif i > 0 and core.casefold() in SMALL:
            out.append(w.lower())
        else:
            out.append(_cap_word(w))
    return " ".join(out)

def film_key(title: str) -> str:
    t = re.sub(r"\([^)]*\)", "", title)            # "Pans Labyrinth (Best of Cinema)" == "Pans Labyrinth"
    return re.sub(r"[^0-9a-zäöüß]+", "", t.casefold())

def cinema_name(raw: str) -> str:
    n = latin1_clean(raw)
    n = smart_case(n)
    if n.islower():
        n = " ".join(w.upper() if w.upper() in ACRONYMS else w.capitalize() for w in n.split(" "))
    n = re.sub(r"\s+München$", "", n, flags=re.I)
    return n

def post(payload: dict) -> dict:
    req = urllib.request.Request(ENDPOINT, data=json.dumps(payload).encode(),
        headers={"content-type": "application/json", "user-agent": "cinema-display/1.0 (private hobby project)"})
    last = None
    for attempt in range(4):
        try:
            with urllib.request.urlopen(req, timeout=30) as r:
                return json.load(r)
        except Exception as e:
            last = e; time.sleep(2 * (attempt + 1))
    raise RuntimeError(f"Kinoheld request failed: {last}")

def fetch_all() -> list:
    shows, page = [], 1
    while page <= 20:
        j = post({"query": QUERY, "variables": {"cinemaProximity": {"city": "muenchen", "distance": 25},
                  "periods": ["TODAY", "TOMORROW"], "excludeExpired": False, "first": 100, "page": page}})
        if j.get("errors"):
            raise RuntimeError(f"GraphQL errors: {j['errors'][:2]}")
        d = j["data"]["programShows"]
        shows += d["data"]
        if not d["paginatorInfo"]["hasMorePages"]:
            break
        page += 1; time.sleep(1)
    return shows

def build(shows: list, today, suburbs=False):
    d = [today.isoformat(), (today + timedelta(days=1)).isoformat()]
    seen, rows = set(), []
    for s in shows:
        cin = s.get("cinema") or {}
        raw = cin.get("name") or ""
        if raw.casefold() in EXCLUDE: continue
        if not suburbs and (cin.get("city") or {}).get("name") != "München": continue
        b = s.get("beginning") or ""
        m = re.match(r"(\d{4}-\d\d-\d\d)T(\d\d):(\d\d)", b)
        if not m or m.group(1) not in d: continue
        day = d.index(m.group(1)); minute = int(m.group(2)) * 60 + int(m.group(3))
        mv = s.get("movie") or {}
        title = latin1_clean(mv.get("title") or s.get("name") or "")
        if not title: continue
        cname = cinema_name(raw)
        key = (cname, day, minute, film_key(title))
        if key in seen: continue
        seen.add(key)
        fl = [f.get("name", "") for f in (s.get("flags") or [])]
        flags = 0
        if "OV" in fl: flags |= FLAG_OV
        if "subtitled OV" in fl: flags |= FLAG_OMU
        if any("3d" in f.casefold() for f in fl): flags |= FLAG_3D
        age = (mv.get("contentRating") or {}).get("minimumAge")
        rows.append((cname, day, minute, title, flags, age))

    # one display title per film key: prefer a variant that wasn't ALL CAPS
    best = {}
    for r in rows:
        k = film_key(r[3])
        if k not in best or (best[k].isupper() and not r[3].isupper()) or \
           (best[k].isupper() == r[3].isupper() and len(r[3]) < len(best[k])):
            best[k] = r[3]
    films = {}
    for r in rows:
        k = film_key(r[3]); t = smart_case(best[k])
        if k not in films: films[k] = [t, 255]
        if r[5] is not None: films[k][1] = r[5]
    cinemas = sorted({r[0] for r in rows}, key=str.casefold)
    film_list = sorted(films.values(), key=lambda x: x[0].casefold())
    cidx = {c: i for i, c in enumerate(cinemas)}
    fidx = {f[0]: i for i, f in enumerate(film_list)}
    out = sorted(((r[1], r[2], cidx[r[0]], fidx[smart_case(best[film_key(r[3])])], r[4]) for r in rows))
    return d, cinemas, film_list, out

def render(d, cinemas, films, shows, now) -> str:
    L = ["KINO1", f"gen={now.strftime('%Y-%m-%dT%H:%M')}", f"d0={d[0]}", f"d1={d[1]}"]
    L += [f"C|{c}" for c in cinemas]
    L += [f"F|{t}|{a}" for t, a in films]
    L += [f"S|{day}|{c}|{f}|{minute}|{fl}" for day, minute, c, f, fl in shows]
    return "\n".join(L) + "\n"

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(os.path.dirname(__file__), "..", "data", "cinemas.txt"))
    ap.add_argument("--input"); ap.add_argument("--suburbs", action="store_true")
    ap.add_argument("--today", help="override date YYYY-MM-DD (tests)")
    a = ap.parse_args()
    now = datetime.now(TZ)
    today = datetime.strptime(a.today, "%Y-%m-%d").date() if a.today else now.date()
    shows = json.load(open(a.input))["data"]["programShows"]["data"] if a.input else fetch_all()
    d, cinemas, films, rows = build(shows, today, a.suburbs)
    if len(rows) < MIN_SHOWS:
        print(f"ERROR: only {len(rows)} shows (< {MIN_SHOWS}); keeping the old file", file=sys.stderr)
        sys.exit(1)
    text = render(d, cinemas, films, rows, now)
    tmp = a.out + ".tmp"
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    open(tmp, "w", encoding="utf-8", newline="\n").write(text)
    os.replace(tmp, a.out)
    print(f"ok: {len(rows)} shows, {len(cinemas)} cinemas, {len(films)} films, {len(text)} bytes -> {a.out}")

if __name__ == "__main__":
    main()
