#include "iniparser.h"

#include <QFile>
#include <QTextStream>
#include <QRegularExpression>

bool isCommentLine(const QString &trimmed)
{
    return trimmed.startsWith(QLatin1Char(';')) || trimmed.startsWith(QLatin1Char('#'));
}

bool parseIni(const QString &path, QStringList &sectionOrder,
              QMap<QString, QVector<IniEntry>> &sections, QString &errorMessage)
{
    sectionOrder.clear();
    sections.clear();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorMessage = file.errorString();
        return false;
    }

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);

    static const QRegularExpression sectionRe(R"(^\[(.+)\]$)");

    QString current;
    bool haveCurrent = false;

    while (!in.atEnd()) {
        const QString line = in.readLine();
        const QString stripped = line.trimmed();

        if (stripped.isEmpty())
            continue;

        const QRegularExpressionMatch m = sectionRe.match(stripped);
        if (m.hasMatch()) {
            current = m.captured(1);
            haveCurrent = true;
            if (!sections.contains(current)) {
                sections.insert(current, {});
                sectionOrder.append(current);
            }
            continue;
        }

        if (!haveCurrent)
            continue;

        if (isCommentLine(stripped)) {
            sections[current].append(IniEntry{stripped, QString()});
            continue;
        }

        const int eq = stripped.indexOf(QLatin1Char('='));
        if (eq >= 0) {
            const QString key = stripped.left(eq);
            const QString value = stripped.mid(eq + 1);
            sections[current].append(IniEntry{key, value});
        } else {
            sections[current].append(IniEntry{stripped, QString()});
        }
    }

    return true;
}

bool writeIni(const QString &path, const QStringList &sectionOrder,
              const QMap<QString, QVector<IniEntry>> &sections, QString &errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        errorMessage = file.errorString();
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    for (const QString &sec : sectionOrder) {
        out << "[" << sec << "]\n";
        const auto &entries = sections.value(sec);
        for (const IniEntry &e : entries) {
            const QString trimmedKey = e.key.trimmed();
            if (isCommentLine(trimmedKey)) {
                out << e.key << "\n";
            } else {
                out << e.key << "=" << e.value << "\n";
            }
        }
        out << "\n";
    }

    return true;
}

QStringList findGearIniFiles(const QString &folder)
{
    QDir dir(folder);
    QStringList result;
    const QFileInfoList entries = dir.entryInfoList({"*.ini"}, QDir::Files);
    for (const QFileInfo &fi : entries) {
        if (fi.fileName().startsWith("gear", Qt::CaseInsensitive))
            result.append(fi.absoluteFilePath());
    }
    std::sort(result.begin(), result.end(), [](const QString &a, const QString &b) {
        return QFileInfo(a).fileName().compare(QFileInfo(b).fileName(), Qt::CaseInsensitive) < 0;
    });
    return result;
}
