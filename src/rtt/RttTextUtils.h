#pragma once
#include <QString>

// Compute the T.140 delta needed to transform `prevSent` into `newText`.
// Returns a string containing:
//   - N × BS (U+0008) for each character that must be deleted from the end
//   - followed by the new characters that must be inserted
// Returns an empty string when prevSent == newText (no change).
//
// This models "common-prefix diff": only changes beyond the longest common
// prefix are encoded as BS + new chars. This covers appending a character,
// deleting a character (backspace), and any mixed edit (paste, cut).
//
// The result is suitable for direct transmission as a T.140 text block.
inline QString rttTxDelta(const QString &prevSent, const QString &newText)
{
    // Find the length of the common prefix.
    int commonLen = 0;
    const int minLen = qMin(prevSent.length(), newText.length());
    while (commonLen < minLen && prevSent[commonLen] == newText[commonLen])
        ++commonLen;

    const int deletions = prevSent.length() - commonLen;
    const QString additions = newText.mid(commonLen);

    if (deletions == 0 && additions.isEmpty())
        return {};

    QString delta;
    delta.reserve(deletions + additions.length());
    for (int i = 0; i < deletions; ++i)
        delta.append(QChar(0x08));  // T.140 BS (U+0008)
    delta.append(additions);
    return delta;
}

// Process one received T.140 text block into an accumulation buffer.
// Rules:
//   - BS (U+0008): remove the last character from the buffer
//   - CR (U+000D) / LF (U+000A): appended as-is so the caller can detect
//     paragraph boundaries and flush to a transcript widget
//   - All other codepoints: append to buffer
// Returns the updated buffer.
inline QString rttRxProcess(const QString &buffer, const QString &incoming)
{
    QString result = buffer;
    for (const QChar ch : incoming) {
        if (ch.unicode() == 0x08) {
            if (!result.isEmpty())
                result.chop(1);
        } else {
            result.append(ch);
        }
    }
    return result;
}
