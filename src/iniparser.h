#pragma once

#include <QString>
#include <QStringList>
#include <QDir>
#include <QFileInfo>

// A single row in a section: (key-or-comment-text, value).
// For comment rows, "first" holds the whole comment line (e.g. "; foo")
// and "second" is unused/empty, mirroring the original tool's model.
struct IniEntry {
    QString key;
    QString value;
};

// One [Section] with its ordered list of entries (including comment rows).
struct IniSection {
    QStringList order;                 // not used directly; kept for symmetry
};

// A whole loaded document (one .ini file).
struct IniDocument {
    QString fileName;                                   // e.g. "GearEngine.ini"
    QString path;                                        // full path on disk
    QStringList sectionOrder;                             // section names, in file order
    QMap<QString, QVector<IniEntry>> sections;            // section name -> rows
    bool dirty = false;
};

// Returns true if the given (trimmed) line starts a comment.
bool isCommentLine(const QString &trimmed);

// Parses an ini file preserving section order, duplicate keys, and comments.
// Returns false (and sets errorMessage) if the file could not be opened.
bool parseIni(const QString &path, QStringList &sectionOrder,
              QMap<QString, QVector<IniEntry>> &sections, QString &errorMessage);

// Writes a document back out in the same duplicate-key-preserving format.
// Returns false (and sets errorMessage) on failure.
bool writeIni(const QString &path, const QStringList &sectionOrder,
              const QMap<QString, QVector<IniEntry>> &sections, QString &errorMessage);

// Finds every "Gear*.ini" file directly inside a folder, sorted case-insensitively.
QStringList findGearIniFiles(const QString &folder);
