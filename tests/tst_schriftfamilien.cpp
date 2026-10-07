// SPDX-License-Identifier: GPL-3.0-or-later
//
// tests/tst_schriftfamilien.cpp  (Longpath)
//
// Ein WAECHTER gegen das Auseinanderlaufen der Schriftwahl.
//
// Anlass, 2026-10-06: der Betreiber sagte „das Rendering ist bei Longpath
// nicht ueberall gleich" und verwies aufs Protokoll. Dort stand:
//
//     Populating font family aliases took N ms. Replace uses of missing
//     font family "SF Mono" with one that exists to avoid this cost.
//
// Nachgezaehlt gab es ACHT Schreibweisen fuer dieselbe Absicht, und auf
// einem Mac kamen dabei VIER verschiedene Schriften heraus: Menlo,
// Monaco, Courier New und -- an zwei Stellen ohne Rueckfallkette --
// die proportionale Vorgabeschrift. Letztere war ausgerechnet die
// Verbindungsanzeige in der Titelleiste und der Protokollbetrachter.
//
// Dieser Test liest den QUELLTEXT. Das ist ungewoehnlich, aber es ist
// die einzige Stelle, an der sich das pruefen laesst: zur Laufzeit
// sieht man nur, was Qt daraus gemacht hat, und Qt macht aus einer
// fehlenden Familie klaglos irgendeine andere.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-06 — Original fuer Longpath, KI-gestuetzt (Anthropic
//                 Claude), Betreiber Martin Fischer.
// =================================================================

#include <QtTest>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

namespace {

QString quellbaum()
{
    const QByteArray env = qgetenv("LONGPATH_SOURCE_DIR");
    if (!env.isEmpty()) {
        return QString::fromLocal8Bit(env) + QStringLiteral("/src");
    }
    return QString();
}

} // namespace

class TestSchriftfamilien : public QObject
{
    Q_OBJECT

private slots:
    // Jede Stilvorlage, die eine Schriftfamilie nennt, muss die zentrale
    // Konstante benutzen. Eine Familie direkt hinzuschreiben ist der Weg,
    // auf dem die acht Schreibweisen entstanden sind.
    void keineSchriftfamilieVonHandInStilvorlagen()
    {
        const QString wurzel = quellbaum();
        if (wurzel.isEmpty()) {
            QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt");
        }
        // "font-family:" gefolgt von irgendetwas, das NICHT sofort das
        // schliessende Anfuehrungszeichen der Konstante ist.
        // Leerzeichen AUSDRUECKLICH, nicht \\s*: mit \\s* darf die Suche
        // null Leerzeichen nehmen und das Leerzeichen selbst als "kein
        // Anfuehrungszeichen" lesen -- dann meldet sie jede richtige
        // Zeile als Fund (beim ersten Lauf genau so passiert).
        static const QRegularExpression vonHand(
            QStringLiteral("font-family: *[^ \"]"));

        QStringList funde;
        QDirIterator it(wurzel, QStringList{QStringLiteral("*.cpp"),
                                            QStringLiteral("*.h")},
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString pfad = it.next();
            QFile f(pfad);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { continue; }
            QTextStream in(&f);
            int zeile = 0;
            while (!in.atEnd()) {
                ++zeile;
                const QString z = in.readLine();
                // Die Erklaerung der Konstante selbst zaehlt nicht mit:
                // dort stehen die alten Schreibweisen mit Absicht, als
                // Beleg. Kommentarzeilen also auslassen.
                const QString beschnitten = z.trimmed();
                if (beschnitten.startsWith(QStringLiteral("//"))
                    || beschnitten.startsWith(QStringLiteral("///"))
                    || beschnitten.startsWith(QStringLiteral("*"))) {
                    continue;
                }
                if (vonHand.match(z).hasMatch()) {
                    funde << QStringLiteral("%1:%2  %3")
                                 .arg(QDir(wurzel).relativeFilePath(pfad))
                                 .arg(zeile)
                                 .arg(beschnitten);
                }
            }
        }
        if (!funde.isEmpty()) {
            QFAIL(qPrintable(
                QStringLiteral(
                    "%1 Stilvorlage(n) nennen eine Schriftfamilie von Hand "
                    "statt LP_MONO_QSS:\n  %2")
                    .arg(funde.size())
                    .arg(funde.join(QStringLiteral("\n  ")))));
        }
    }

    // Eine QFont mit EINER Familie und ohne Rueckfallkette ist die zweite
    // Art, wie es schieflaeuft: fehlt die Familie, nimmt Qt die
    // proportionale Vorgabe, und niemand merkt es.
    void keineQFontOhneRueckfallkette()
    {
        const QString wurzel = quellbaum();
        if (wurzel.isEmpty()) {
            QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt");
        }
        // QFont("Irgendeine Schrift" ...) -- nur benannte Familien, nicht
        // QFont(), QFont(base), QFont(f) usw.
        // ZWEI Wege, eine Familie ohne Rueckfall zu nennen -- das Muster
        // sah bis zum 2026-10-07 nur den ersten:
        //
        //   QFont("SF Mono", 10)          <- Konstruktor
        //   f.setFamily("SF Mono")        <- Setzer, 11 Fundstellen
        //
        // Der Ausgangsfall vom 2026-10-06 (Titelleiste, Protokoll-
        // betrachter) haette sich also als setFamily() an derselben
        // Pruefung vorbeigeschrieben. setFamilies() mit der Kette ist
        // ausdruecklich erlaubt.
        static const QRegularExpression ohneKette(
            QStringLiteral("QFont\\s*\\(\\s*(QStringLiteral\\s*\\(\\s*)?\""
                           "|\\bsetFamily\\s*\\(\\s*(QStringLiteral\\s*\\(\\s*)?\""));

        QStringList funde;
        QDirIterator it(wurzel, QStringList{QStringLiteral("*.cpp"),
                                            QStringLiteral("*.h")},
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString pfad = it.next();
            QFile f(pfad);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { continue; }
            QTextStream in(&f);
            int zeile = 0;
            while (!in.atEnd()) {
                ++zeile;
                const QString z = in.readLine();
                const QString beschnitten = z.trimmed();
                if (beschnitten.startsWith(QStringLiteral("//"))
                    || beschnitten.startsWith(QStringLiteral("*"))) {
                    continue;
                }
                if (ohneKette.match(z).hasMatch()) {
                    funde << QStringLiteral("%1:%2  %3")
                                 .arg(QDir(wurzel).relativeFilePath(pfad))
                                 .arg(zeile)
                                 .arg(beschnitten);
                }
            }
        }
        if (!funde.isEmpty()) {
            QFAIL(qPrintable(
                QStringLiteral(
                    "%1 QFont(...) nennt eine Familie ohne Rueckfallkette. "
                    "Style::monoFont() benutzen (setFamilies), sonst nimmt "
                    "Qt bei fehlender Schrift klaglos die proportionale "
                    "Vorgabe:\n  %2")
                    .arg(funde.size())
                    .arg(funde.join(QStringLiteral("\n  ")))));
        }
    }
};

QTEST_MAIN(TestSchriftfamilien)
#include "tst_schriftfamilien.moc"
