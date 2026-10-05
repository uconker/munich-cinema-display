#pragma once
// Pure C++ (no Arduino): parses data/cinemas.txt and builds the screen lines.
// Host-testable: see test/host_test.cpp
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXC 48
#define MAXF 160
#define MAXS 900
#define CIN_LEN 32
#define FILM_LEN 44
#define FLAG_OV 1
#define FLAG_OMU 2
#define FLAG_3D 4
#define COLS 25          // characters per line with the 5x8 font
#define MAXL 140         // lines per page
#define SOON_MIN 45      // "starts soon" marker window (minutes)
#define GRACE_MIN 5      // a show that started <= 5 min ago is still listed

struct Show { uint8_t day, c, f, flags; uint16_t min; };

struct CinemaData {
  char gen[20], d0[12], d1[12];
  int nC, nF, nS;
  char cin[MAXC][CIN_LEN];
  char film[MAXF][FILM_LEN];
  uint8_t age[MAXF];
  Show show[MAXS];
  bool valid;
  void clear() { memset(this, 0, sizeof(*this)); }
};

// ---------- UTF-8 helpers ----------
inline void copyUtf8(char *dst, int cap, const char *src, int len) {
  if (len > cap - 1) len = cap - 1;
  while (len > 0 && ((uint8_t)src[len] & 0xC0) == 0x80) len--;  // don't cut a multibyte char
  memcpy(dst, src, len);
  dst[len] = 0;
}
inline int utf8Len(const char *s) {
  int n = 0;
  for (; *s; s++) if (((uint8_t)*s & 0xC0) != 0x80) n++;
  return n;
}
// copy at most maxChars characters (not bytes); adds nothing
inline void clipChars(char *dst, int cap, const char *src, int maxChars) {
  int chars = 0, o = 0;
  for (const char *p = src; *p; ) {
    int l = ((uint8_t)*p < 0x80) ? 1 : ((uint8_t)*p < 0xE0) ? 2 : ((uint8_t)*p < 0xF0) ? 3 : 4;
    if (chars >= maxChars || o + l >= cap) break;
    for (int i = 0; i < l && p[i]; i++) dst[o++] = p[i];
    p += l; chars++;
  }
  dst[o] = 0;
}

// ---------- parser ----------
inline bool parseCinemaData(const char *text, size_t len, CinemaData &d) {
  d.clear();
  const char *p = text, *end = text + len;
  bool header = false;
  while (p < end) {
    const char *e = (const char *)memchr(p, '\n', end - p);
    if (!e) e = end;
    int L = (int)(e - p);
    if (L > 0 && p[L - 1] == '\r') L--;
    if (L > 0) {
      if (!header) { header = (L >= 5 && !strncmp(p, "KINO1", 5)); if (!header) return false; }
      else if (L > 4 && !strncmp(p, "gen=", 4)) copyUtf8(d.gen, sizeof(d.gen), p + 4, L - 4);
      else if (L > 3 && !strncmp(p, "d0=", 3)) copyUtf8(d.d0, sizeof(d.d0), p + 3, L - 3);
      else if (L > 3 && !strncmp(p, "d1=", 3)) copyUtf8(d.d1, sizeof(d.d1), p + 3, L - 3);
      else if (L > 2 && p[0] == 'C' && p[1] == '|') {
        if (d.nC < MAXC) { copyUtf8(d.cin[d.nC], CIN_LEN, p + 2, L - 2); d.nC++; }
      } else if (L > 2 && p[0] == 'F' && p[1] == '|') {
        if (d.nF < MAXF) {
          int last = L - 1; while (last > 1 && p[last] != '|') last--;     // age = after the last '|'
          copyUtf8(d.film[d.nF], FILM_LEN, p + 2, last - 2);
          char tmp[8]; copyUtf8(tmp, sizeof(tmp), p + last + 1, L - last - 1);
          d.age[d.nF] = (uint8_t)atoi(tmp); d.nF++;
        }
      } else if (L > 2 && p[0] == 'S' && p[1] == '|') {
        char buf[48]; copyUtf8(buf, sizeof(buf), p + 2, L - 2);
        int v[5], n = 0; char *q = buf;
        while (n < 5) { char *x; v[n++] = (int)strtol(q, &x, 10); if (*x != '|') break; q = x + 1; }
        if (n == 5 && d.nS < MAXS && v[0] >= 0 && v[0] <= 1 && v[1] >= 0 && v[1] < d.nC &&
            v[2] >= 0 && v[2] < d.nF && v[3] >= 0 && v[3] < 1440) {
          Show &s = d.show[d.nS++];
          s.day = v[0]; s.c = v[1]; s.f = v[2]; s.min = v[3]; s.flags = v[4];
        }
      }
    }
    p = e + 1;
  }
  d.valid = header && d.nS > 0 && d.d0[0] && d.d1[0] && d.nC > 0;
  return d.valid;
}

// ---------- "what is shown right now" ----------
struct View { int today; int viewDay; int nowMin; };   // today: index of today's date in data (-1 none); viewDay: day shown (-1 none)

inline int dayIndexOf(const CinemaData &d, const char *iso) {
  if (!strcmp(iso, d.d0)) return 0;
  if (!strcmp(iso, d.d1)) return 1;
  return -1;
}
inline bool showVisible(const View &v, const Show &s) {
  if ((int)s.day != v.viewDay) return false;
  if (v.viewDay == v.today && (int)s.min < v.nowMin - GRACE_MIN) return false;
  return true;
}
inline View makeView(const CinemaData &d, const char *todayIso, int nowMin) {
  View v; v.nowMin = nowMin; v.today = dayIndexOf(d, todayIso); v.viewDay = -1;
  if (v.today < 0) {
    if (strcmp(todayIso, d.d0) < 0) v.viewDay = 0;      // data is from the "future" (clock off): show it anyway
    return v;                                          // else data too old -> viewDay -1
  }
  for (int i = 0; i < d.nS; i++)
    if (d.show[i].day == v.today && (int)d.show[i].min >= nowMin - GRACE_MIN) { v.viewDay = v.today; return v; }
  v.viewDay = (v.today == 0) ? 1 : -1;                  // nothing left today -> tomorrow (if we have it)
  return v;
}
inline bool isSoon(const View &v, const Show &s) {
  if (v.viewDay != v.today) return false;
  int diff = (int)s.min - v.nowMin;
  return diff >= -GRACE_MIN && diff <= SOON_MIN;
}

// ---------- pages ----------
enum Mode { MODE_CINEMA = 0, MODE_FILM = 1, MODE_SOON = 2 };

// ids of cinemas (mode cinema) or films (mode film) that have at least one visible show
inline int buildPages(const CinemaData &d, const View &v, int mode, int *out, int cap) {
  static bool seen[MAXF > MAXC ? MAXF : MAXC];
  int n = 0;
  if (mode == MODE_SOON) { out[0] = 0; return 1; }
  int lim = (mode == MODE_CINEMA) ? d.nC : d.nF;
  for (int i = 0; i < lim; i++) seen[i] = false;
  for (int i = 0; i < d.nS; i++) {
    const Show &s = d.show[i];
    if (!showVisible(v, s)) continue;
    int id = (mode == MODE_CINEMA) ? s.c : s.f;
    seen[id] = true;
  }
  for (int i = 0; i < lim && n < cap; i++) if (seen[i]) out[n++] = i;
  return n;
}

struct Line { char t[56]; uint8_t style; };   // style 1 = heading
struct Lines { Line l[MAXL]; int n; };

inline void addLine(Lines &L, const char *s, uint8_t style) {
  if (L.n >= MAXL) return;
  clipChars(L.l[L.n].t, sizeof(L.l[L.n].t), s, COLS);
  L.l[L.n].style = style; L.n++;
}

inline const char *variantPrefix(int key, char *buf) {
  buf[0] = 0;
  if (key & FLAG_3D) strcat(buf, "3D ");
  if (key & FLAG_OV) strcat(buf, "OV ");
  if (key & FLAG_OMU) strcat(buf, "OmU ");
  return buf;
}

// the time lines for one (cinema, film) pair: groups by variant (deutsch / OV / OmU / 3D), wrapped at COLS
inline void addTimeLines(Lines &L, const CinemaData &d, const View &v, int c, int f) {
  for (int key = 0; key < 8; key++) {
    char line[64]; int pos = 0; bool any = false; char pre[16];
    variantPrefix(key, pre);
    auto flush = [&]() { if (pos > 0) { line[pos] = 0; addLine(L, line, 0); } pos = 0; any = true; };
    for (int i = 0; i < d.nS; i++) {
      const Show &s = d.show[i];
      if (s.c != c || s.f != f || (s.flags & 7) != key || !showVisible(v, s)) continue;
      char tok[10]; snprintf(tok, sizeof(tok), isSoon(v, s) ? "%02d:%02d*" : "%02d:%02d", s.min / 60, s.min % 60);
      int tl = (int)strlen(tok);
      if (pos == 0) { pos = snprintf(line, sizeof(line), "  %s", pre); }
      else if (pos + 1 + tl > COLS) { flush(); pos = snprintf(line, sizeof(line), "  %s", pre); }
      else line[pos++] = ' ';
      memcpy(line + pos, tok, tl); pos += tl;
    }
    if (pos > 0) flush();
    (void)any;
  }
}

inline void dateLabel(const CinemaData &d, int day, char *out, int cap) {   // "Morgen 07.10."
  const char *iso = day ? d.d1 : d.d0;                                      // YYYY-MM-DD
  snprintf(out, cap, "%.2s.%.2s.", iso + 8, iso + 5);
}

// lines for cinema page `c`: one block per film, ordered by first visible start
inline void buildCinemaLines(Lines &L, const CinemaData &d, const View &v, int c) {
  L.n = 0;
  if (v.viewDay != v.today) { char b[24], l[40]; dateLabel(d, v.viewDay, b, sizeof(b)); snprintf(l, sizeof(l), "Morgen %s", b); addLine(L, l, 0); }
  static bool done[MAXF]; memset(done, 0, sizeof(done));
  for (int i = 0; i < d.nS; i++) {
    const Show &s = d.show[i];
    if (s.c != c || done[s.f] || !showVisible(v, s)) continue;
    done[s.f] = true;
    addLine(L, d.film[s.f], 1);
    addTimeLines(L, d, v, c, s.f);
  }
}

inline void buildFilmLines(Lines &L, const CinemaData &d, const View &v, int f) {
  L.n = 0;
  static bool done[MAXC]; memset(done, 0, sizeof(done));
  int cnt = 0;
  for (int i = 0; i < d.nS; i++) { const Show &s = d.show[i]; if (s.f == f && showVisible(v, s) && !done[s.c]) { done[s.c] = true; cnt++; } }
  memset(done, 0, sizeof(done));
  char info[40];
  if (d.age[f] != 255) snprintf(info, sizeof(info), "FSK %d - %d Kino%s", d.age[f], cnt, cnt == 1 ? "" : "s");
  else snprintf(info, sizeof(info), "%d Kino%s", cnt, cnt == 1 ? "" : "s");
  if (v.viewDay != v.today) { char b[24]; dateLabel(d, v.viewDay, b, sizeof(b)); char l[96]; snprintf(l, sizeof(l), "Morgen %s - %s", b, info); addLine(L, l, 0); }
  else addLine(L, info, 0);
  for (int i = 0; i < d.nS; i++) {
    const Show &s = d.show[i];
    if (s.f != f || done[s.c] || !showVisible(v, s)) continue;
    done[s.c] = true;
    addLine(L, d.cin[s.c], 1);
    addTimeLines(L, d, v, s.c, f);
  }
}

// "starting soon" across the whole city: 2 lines per show
inline void buildSoonLines(Lines &L, const CinemaData &d, const View &v) {
  L.n = 0;
  int shown = 0, limit = (v.viewDay == v.today) ? 90 : 100000;
  if (v.viewDay != v.today) { char b[24], l[40]; dateLabel(d, v.viewDay, b, sizeof(b)); snprintf(l, sizeof(l), "Morgen %s ab:", b); addLine(L, l, 0); }
  for (int i = 0; i < d.nS && shown < 40; i++) {
    const Show &s = d.show[i];
    if (!showVisible(v, s)) continue;
    if (v.viewDay == v.today && (int)s.min - v.nowMin > limit) continue;
    char l1[64], l2[64], ft[40], pre[16];
    clipChars(ft, sizeof(ft), d.film[s.f], COLS - 6);
    snprintf(l1, sizeof(l1), "%02d:%02d %s", s.min / 60, s.min % 60, ft);
    variantPrefix(s.flags & 7, pre);
    char cn[40]; clipChars(cn, sizeof(cn), d.cin[s.c], COLS - 2 - (int)strlen(pre));
    snprintf(l2, sizeof(l2), "  %s%s", pre, cn);
    addLine(L, l1, 1); addLine(L, l2, 0); shown++;
  }
}
