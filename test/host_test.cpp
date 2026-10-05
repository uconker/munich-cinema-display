#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <fstream>
#include <sstream>
#include "../board/src/data.h"

static CinemaData D;
static Lines L;

static int findC(const char *s) { for (int i = 0; i < D.nC; i++) if (strstr(D.cin[i], s)) return i; return -1; }
static int findF(const char *s) { for (int i = 0; i < D.nF; i++) if (strstr(D.film[i], s)) return i; return -1; }
static void dump(const char *title, int from, int rows = 6) {
  printf("+-------------------------+ %s\n", title);
  for (int r = 0; r < rows; r++) {
    int i = from + r;
    printf("|%-25s|\n", i < L.n ? L.l[i].t : "");
  }
  printf("+-------------------------+\n");
}

int main() {
  std::ifstream f("sample_cinemas.txt"); std::stringstream ss; ss << f.rdbuf(); std::string txt = ss.str();
  assert(parseCinemaData(txt.c_str(), txt.size(), D));
  printf("parsed: %d cinemas, %d films, %d shows, d0=%s d1=%s gen=%s\n", D.nC, D.nF, D.nS, D.d0, D.d1, D.gen);
  assert(D.nC == 19 && D.nF == 31 && D.nS == 103 && !strcmp(D.d0, "2026-10-06") && !strcmp(D.d1, "2026-10-07"));

  // garbage / truncated input must be rejected, never crash
  CinemaData G; assert(!parseCinemaData("hello", 5, G));
  assert(!parseCinemaData("KINO1\ngen=x\n", 12, G));
  std::string cut = txt.substr(0, txt.size() / 2);                 // download cut in half: still parses what it has, no crash
  CinemaData H; parseCinemaData(cut.c_str(), cut.size(), H);
  std::string bad = "KINO1\ngen=a\nd0=2026-10-06\nd1=2026-10-07\nC|X\nF|Y|12\nS|0|9|9|99999|0\nS|0|0|0|600|0\n";
  CinemaData B; assert(parseCinemaData(bad.c_str(), bad.size(), B) && B.nS == 1);   // out-of-range show dropped

  // ---- 18:40 on 2026-10-06 ----
  View v = makeView(D, "2026-10-06", 18 * 60 + 40);
  assert(v.today == 0 && v.viewDay == 0);
  int pages[MAXF];
  int nc = buildPages(D, v, MODE_CINEMA, pages, MAXF), nf = buildPages(D, v, MODE_FILM, pages, MAXF);
  printf("at 18:40: %d cinemas, %d films with shows left\n", nc, nf);

  int mat = findC("Mathäser"); assert(mat >= 0);
  buildCinemaLines(L, D, v, mat);
  printf("\nCINEMA mode, Mathaeser, %d lines total\n", L.n);
  dump("window 1", 0); dump("window 2", 6);
  for (int i = 0; i < L.n; i++) assert(utf8Len(L.l[i].t) <= COLS);
  for (int i = 0; i < L.n; i++) if (L.l[i].style == 0 && L.l[i].t[2] >= '0') {} // time lines exist

  int dig = findF("Digger"); assert(dig >= 0);
  buildFilmLines(L, D, v, dig);
  printf("\nFILM mode, Digger, %d lines\n", L.n);
  dump("window 1", 0); dump("window 2", 6);

  buildSoonLines(L, D, v);
  printf("\nSOON mode (next 90 min), %d lines\n", L.n);
  dump("window 1", 0); dump("window 2", 6);

  // past shows are hidden: at 18:40 nothing before 18:35 may appear
  for (int i = 0; i < D.nS; i++) if (D.show[i].day == 0 && D.show[i].min < 18 * 60 + 35) assert(!showVisible(v, D.show[i]));
  // soon marker
  int soonCount = 0; for (int i = 0; i < D.nS; i++) if (isSoon(v, D.show[i])) soonCount++;
  printf("\nshows marked soon (<=45 min): %d\n", soonCount); assert(soonCount > 0);

  // OV grouping
  int mus = findC("Museum");
  buildCinemaLines(L, D, v, mus);
  printf("\nCINEMA mode, Museum Lichtspiele (OV variants):\n"); dump("window 1", 0);

  // ---- late evening: after the last show today -> tomorrow ----
  View late = makeView(D, "2026-10-06", 23 * 60 + 30);
  assert(late.today == 0 && late.viewDay == 1);
  buildCinemaLines(L, D, late, mat);
  printf("\n23:30 -> tomorrow view, Mathaeser:\n"); dump("window 1", 0);

  // ---- just after midnight on d1: 00:30 show still visible at 00:10 ----
  View mid = makeView(D, "2026-10-07", 10);
  assert(mid.today == 1 && mid.viewDay == 1);
  // ---- day after d1: stale ----
  View stale = makeView(D, "2026-10-08", 600); assert(stale.viewDay == -1);
  // ---- all gone on the last day ----
  View end = makeView(D, "2026-10-07", 23 * 60 + 50); assert(end.viewDay == -1);

  // UTF-8 safe clipping
  char out[40]; clipChars(out, sizeof(out), "Über Unterbiberger - vom Woid", 6); assert(!strcmp(out, "Über U"));
  char cp[6]; copyUtf8(cp, sizeof(cp), "abcdÄ", 5); assert(!strcmp(cp, "abcd"));   // would cut Ä in half -> backs up
  printf("\nALL TESTS PASSED\n");
}
