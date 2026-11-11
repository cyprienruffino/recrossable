#ifndef JSON_H
#define JSON_H

#include <cstdio>
#include <string>

namespace json {

inline std::string quote(const std::string &s)
{
    std::string out = "\"";
    for (const unsigned char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char escaped[7];
                std::snprintf(escaped, sizeof escaped, "\\u%04x", c);
                out += escaped;
            } else {
                out += char(c);
            }
        }
    }
    return out + '"';
}

} // namespace json

#endif // JSON_H
