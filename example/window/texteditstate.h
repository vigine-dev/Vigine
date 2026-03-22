#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// Simple UTF-8 text buffer with cursor for 3D in-world text editing.
// Shared between SetupTextEditTask (creates entity) and TextEditorSystem (handles input).
struct TextEditState
{
    std::string text;
    std::size_t cursorPos{0};
    std::size_t editBytePos{0}; // byte offset where last edit occurred (for incremental rebuild)
    bool showCursor{true};
    bool dirty{true};       // true = any visual update is needed this frame
    bool textChanged{true}; // true = text content or cursor position changed (not just blink)
    bool textContentChanged{
        false}; // true = actual text content changed (insert/delete), not just cursor move

    // Insert a Unicode code point (UTF-8 encoded) at cursor position.
    void insertCodepoint(uint32_t cp)
    {
        if (cp < 0x20 && cp != '\t' && cp != '\n')
            return; // skip control characters

        char buf[5] = {};
        size_t len  = 0;
        if (cp < 0x80u)
        {
            buf[0] = static_cast<char>(cp);
            len    = 1;
        } else if (cp < 0x800u)
        {
            buf[0] = static_cast<char>(0xC0u | (cp >> 6));
            buf[1] = static_cast<char>(0x80u | (cp & 0x3Fu));
            len    = 2;
        } else if (cp < 0x10000u)
        {
            buf[0] = static_cast<char>(0xE0u | (cp >> 12));
            buf[1] = static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
            buf[2] = static_cast<char>(0x80u | (cp & 0x3Fu));
            len    = 3;
        } else if (cp < 0x110000u)
        {
            buf[0] = static_cast<char>(0xF0u | (cp >> 18));
            buf[1] = static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu));
            buf[2] = static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
            buf[3] = static_cast<char>(0x80u | (cp & 0x3Fu));
            len    = 4;
        } else
        {
            return; // invalid code point
        }

        text.insert(cursorPos, buf, len);
        editBytePos         = cursorPos;
        cursorPos          += len;
        dirty               = true;
        textChanged         = true;
        textContentChanged  = true;
    }

    // Insert UTF-8 bytes at cursor position (used by clipboard paste).
    void insertUtf8(const std::string &utf8)
    {
        if (utf8.empty())
            return;

        text.insert(cursorPos, utf8);
        editBytePos         = cursorPos;
        cursorPos          += utf8.size();
        dirty               = true;
        textChanged         = true;
        textContentChanged  = true;
    }

    // Remove the UTF-8 character immediately before the cursor.
    void backspace()
    {
        if (cursorPos == 0)
            return;
        size_t pos = cursorPos;
        do
        {
            --pos;
        } while (pos > 0 && (static_cast<unsigned char>(text[pos]) & 0xC0u) == 0x80u);
        text.erase(pos, cursorPos - pos);
        editBytePos        = pos;
        cursorPos          = pos;
        dirty              = true;
        textChanged        = true;
        textContentChanged = true;
    }

    // Remove the UTF-8 character immediately after the cursor.
    void deleteChar()
    {
        if (cursorPos >= text.size())
            return;
        size_t end = cursorPos + 1;
        while (end < text.size() && (static_cast<unsigned char>(text[end]) & 0xC0u) == 0x80u)
            ++end;
        text.erase(cursorPos, end - cursorPos);
        editBytePos        = cursorPos;
        dirty              = true;
        textChanged        = true;
        textContentChanged = true;
    }

    // Move cursor one UTF-8 character to the left.
    void moveCursorLeft()
    {
        if (cursorPos == 0)
            return;
        size_t pos = cursorPos;
        do
        {
            --pos;
        } while (pos > 0 && (static_cast<unsigned char>(text[pos]) & 0xC0u) == 0x80u);
        cursorPos   = pos;
        dirty       = true;
        textChanged = true;
    }

    // Move cursor one UTF-8 character to the right.
    void moveCursorRight()
    {
        if (cursorPos >= text.size())
            return;
        ++cursorPos;
        while (cursorPos < text.size() &&
               (static_cast<unsigned char>(text[cursorPos]) & 0xC0u) == 0x80u)
            ++cursorPos;
        dirty       = true;
        textChanged = true;
    }
};
