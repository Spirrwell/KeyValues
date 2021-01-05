# KeyValues

KeyValues is a serialization format used in Source Engine. This is my own KeyValues library for made in C++17. It's not the fastest, but it's built to be syntactically simple.

This is maybe a bit more flexible than Valve's format, but it doesn't support UTF-16-LE which is used for translations files in Source.

Current Features:
- [x] UTF-8
- [x] Multi-key support (can have multiple keys of the same name)
- [x] Single-line comments //
- [x] Mutli-line comments /**/
- [x] Basic parsing error checking with messages piped to a debug callback if set.
