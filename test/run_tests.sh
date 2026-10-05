#!/bin/sh
# Offline tests (no network, no board). Run from the test/ folder.
set -e
python3 make_mock.py
python3 ../scraper/scrape.py --input mock_api.json --today 2026-10-06 --out sample_cinemas.txt
g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined host_test.cpp -o host_test && ./host_test | tail -1
g++ -std=c++17 -I stubs -Wall -fsanitize=address,undefined gesture_test.cpp -o gesture_test 2>/dev/null && ./gesture_test
