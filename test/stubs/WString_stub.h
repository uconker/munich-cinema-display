#pragma once
struct String { std::string s; size_t length() const { return s.size(); } const char* c_str() const { return s.c_str(); } };
