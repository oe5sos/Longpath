// SPDX-License-Identifier: GPL-3.0-or-later
//
// „englisch durchgehend, aber deutsch als option." (Betreiber,
// 2026-10-14)
//
// Am 15.10. habe ich ihm gemeldet, nach dem Umstellungsdurchgang gebe
// es „0 deutsche Quelltexte". Das war ZWEIMAL falsch: im
// Bildschirmfoto der Applet-Spalte standen LAUTSTÄRKE,
// RAUSCHMINDERUNG, VORLAUF und Leistung, und im Panadapter-Menue
// „Hintergrundbild", „Bild sichtbar", „Grundfarbe…". Der Durchgang
// hatte nur erwischt, was schon durch tr() lief.
//
// Eine Behauptung, die zweimal gefallen ist, gehoert nicht noch einmal
// in eine Nachricht, sondern in einen Prueflauf. Dieser hier liest die
// Quellen unter src/gui, sammelt die SICHTBAREN Zeichenketten und
// zaehlt die mit deutschen Merkmalen. Die Zahl darf sinken, nie
// steigen — dieselbe Sperrklinke wie bei den namenlosen Farben und bei
// „Nie Radius 3".
//
// Was er NICHT prueft: Kommentare (deutsch, mit Absicht), Protokoll-
// und Fehlertexte unter src/core (die liest der Betreiber, nicht der
// Benutzer), und Stylesheets.
//
// ── Und was diese Zahl NICHT ist ────────────────────────────────────
//
// Sie ist „deutsche Texte, die DIESE Liste erkennt" — nicht „deutsche
// Texte". Am 2026-10-15 habe ich dem Betreiber „noch 4" gemeldet; der
// vollstaendige ctest-Lauf brachte danach 31 weitere zutage, die die
// Liste schlicht nicht kannte („Seitenbereich", „Statuszeile",
// „Notch bearbeiten…", „Alle trennen" …). Die Liste ist ein NETZ,
// kein Beweis. Wer hier eine Zahl meldet, sagt dazu, womit sie
// gemessen wurde.
//
// ── Das Loch ist inzwischen BENANNT und GEMESSEN (2026-10-17) ───────
//
// Dieser Lauf liest ZEILENWEISE. In einer einzelnen Zeile faengt `\s`
// keinen Zeilenumbruch, also sieht er nichts, wenn die Klammer am
// Zeilenende steht und das Anfuehrungszeichen erst darunter:
//
//     m_bgPathLabel->setToolTip(QStringLiteral(
//         "Bild hinter Raster und Spur. Wird auf die Flaeche "
//         "vergroessert und mittig beschnitten."));
//
// Genau so werden lange Hinweistexte geschrieben -- es trifft also
// bevorzugt die laengsten. Am 2026-10-17 nachgezaehlt: zu den 9 hier
// gemeldeten kommen **47 weitere** in src/gui, die durchrutschen.
// Schwerpunkte: SwrSweepPanel.cpp (14), DisplaySetupPages.cpp (7),
// AntennaWindow.cpp (5).
//
// Behoben wird es NICHT durch Heben der Decke, sondern indem der Lauf
// die Datei als Ganzes liest und die "..."-Stuecke eines Rufs
// zusammensetzt -- und indem die 47 uebersetzt werden. Beides gehoert
// in einen eigenen Durchgang, NACH dem Mergen von #216/#221/#222:
// alle drei fassen MainWindow.cpp an, und ein weiterer main-Zweig
// darauf macht aus drei geloesten Konflikten drei neue.
//
// Bis dahin heisst die Zahl hier: „deutsche Texte, die dieses Netz
// mit diesem Loch erkennt". Nicht „deutsche Texte".

#include <QtTest>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

namespace {

/// ── Muster statt Wortliste (2026-10-15, zweiter Anlauf) ────────────
///
/// Der erste Anlauf fuehrte eine Liste deutscher Woerter. Sie meldete
/// „noch 4", und der vollstaendige ctest-Lauf brachte danach 35
/// weitere zutage, die sie nicht kannte — „Seitenbereich",
/// „Statuszeile", „Spitzenhaltung", „Quelle", „Halten". Eine Liste
/// faengt nur, was jemand hineingeschrieben hat.
///
/// Diese Muster faengt mehr: Umlaute, und Buchstabenfolgen, die im
/// Englischen praktisch nicht vorkommen — -ung, -keit, -heit, -lich,
/// -isch, -chen, sch-, dazu die haeufigsten kurzen Woerter. Ein
/// Fehlalarm kostet eine Uebersetzung, die niemand braucht; ein
/// Durchrutscher kostet eine falsche Meldung an den Betreiber.
const QRegularExpression& deutschesMuster()
{
    static const QRegularExpression re(
        // „sch" und „\\bam\\b" standen hier zuerst und fingen vor allem
        // Englisch: scheme, schematic, AM. Raus damit — ein Netz, das
        // zu oft falsch anschlaegt, wird nicht gelesen.
        QStringLiteral("(ung\\b|keit\\b|heit\\b|lich\\b|isch\\b|chen\\b|"
                       "zeit\\b|zahl\\b|\\bzu\\b|\\bein\\b|\\bder\\b|\\bdie\\b|"
                       "\\bdas\\b|\\bund\\b|\\bmit\\b|\\bvon\\b|\\bnicht\\b|"
                       "\\bkein|\\balle\\b|\\bnur\\b|\\bauf\\b|\\bim\\b|"
                       "\\bals\\b|\\bwie\\b|\\bist\\b|\\bzum\\b|\\bzur\\b|"
                       "Zeichen|Deutsch)"),
        QRegularExpression::CaseInsensitiveOption);
    return re;
}

bool riechtDeutsch(const QString& t)
{
    for (const QChar c : t) {
        if (c == QChar(u'\u00e4') || c == QChar(u'\u00f6') || c == QChar(u'\u00fc')
            || c == QChar(u'\u00c4') || c == QChar(u'\u00d6') || c == QChar(u'\u00dc')
            || c == QChar(u'\u00df')) {
            return true;
        }
    }
    return deutschesMuster().match(t).hasMatch();
}

} // namespace

class TstOberflaecheEnglisch : public QObject
{
    Q_OBJECT

private slots:
    void dieZahlDarfSinkenNieSteigen()
    {
        const QString wurzel =
            QStringLiteral(LONGPATH_SRC_DIR) + QStringLiteral("/src/gui");
        QVERIFY2(QDir(wurzel).exists(), qPrintable(wurzel));

        // tr("…"), QStringLiteral("…"), QLatin1String("…") — alles, was
        // als Text im Fenster landen kann.
        // Ein Ruf, EIN oder MEHRERE Literale dahinter. Der Umbau vom
        // 2026-10-18: bis dahin las dieser Lauf zeilenweise und
        // verlangte das Anfuehrungszeichen auf DERSELBEN Zeile wie die
        // Klammer. Steht die Klammer am Zeilenende -- die uebliche Form
        // fuer lange Hinweistexte --, sah er nichts:
        //
        //     m_bgPathLabel->setToolTip(QStringLiteral(
        //         "Bild hinter Raster und Spur. Wird auf die Flaeche "
        //         "vergroessert und mittig beschnitten."));
        //
        // Gemessen am 2026-10-17: zu den damals gemeldeten 9 kamen so
        // 47 weitere, die durchrutschten. Jetzt wird die Datei als
        // GANZES gelesen und die Stuecke werden zusammengesetzt.
        static const QRegularExpression sichtbar(
            QStringLiteral("(?:\\btr|QStringLiteral|QLatin1String)"
                           "\\(\\s*((?:\"(?:[^\"\\\\]|\\\\.)*\"\\s*)+)"));
        static const QRegularExpression stueck(
            QStringLiteral("\"((?:[^\"\\\\]|\\\\.)*)\""));

        QStringList funde;
        QDirIterator it(wurzel, {QStringLiteral("*.cpp"), QStringLiteral("*.h")},
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString pfad = it.next();
            QFile f(pfad);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { continue; }
            const QString ganz = QString::fromUtf8(f.readAll());

            // Kommentar- und Protokollzeilen NEUTRALISIEREN statt
            // ueberspringen: so bleibt die Datei ein zusammenhaengender
            // Text (mehrzeilige Literale bleiben also lesbar), und die
            // Zeilennummern stimmen weiter, weil nur der Inhalt durch
            // Leerzeichen ersetzt wird.
            QStringList zeilen = ganz.split(QLatin1Char('\n'));
            for (QString& z : zeilen) {
                const QString roh = z.trimmed();
                const bool raus =
                    roh.startsWith(QLatin1String("//"))
                    || roh.startsWith(QLatin1Char('*'))
                    || roh.startsWith(QLatin1String("qC"))
                    || roh.startsWith(QLatin1String("qDebug"))
                    || roh.startsWith(QLatin1String("qInfo"))
                    || roh.startsWith(QLatin1String("qWarning"))
                    || roh.startsWith(QLatin1String("<<"));
                if (raus) { z = QString(z.size(), QLatin1Char(' ')); }
            }
            const QString text = zeilen.join(QLatin1Char('\n'));

            {
                auto m = sichtbar.globalMatch(text);
                while (m.hasNext()) {
                    const QRegularExpressionMatch tref = m.next();
                    // Die Stuecke eines Rufs zusammensetzen -- ein
                    // Hinweistext ueber vier Zeilen ist EIN Text.
                    QString t;
                    auto st = stueck.globalMatch(tref.captured(1));
                    while (st.hasNext()) { t += st.next().captured(1); }
                    const int zeile =
                        int(QStringView(text).left(tref.capturedStart())
                                .count(QLatin1Char('\n'))) + 1;
                    if (t.size() < 3) { continue; }
                    if (t.startsWith(QLatin1Char('#'))) { continue; }
                    // Stylesheets sind kein Text im Fenster.
                    if (t.contains(QLatin1Char('{'))
                        && t.contains(QLatin1Char(':'))) {
                        continue;
                    }
                    if (!riechtDeutsch(t)) { continue; }
                    funde << QStringLiteral("%1:%2: %3")
                                 .arg(QFileInfo(pfad).fileName())
                                 .arg(zeile)
                                 .arg(t.left(60));
                }
            }
        }

        std::sort(funde.begin(), funde.end());
        qInfo().noquote() << "Deutsche sichtbare Texte in src/gui:"
                          << funde.size();
        for (const QString& f : funde) { qInfo().noquote() << "   " << f; }

        // Stand nach dem Durchgang vom 2026-10-15: von 96 auf 4.
        //
        // Die neun, die bleiben, sind acht SCHLAGWOERTER der
        // Widget-Suche (anpassung, daempfung, leistung, linearisierung,
        // vorverzerrung, fernsteuerung, spannung) und der Name der
        // deutschen Sprache in der Sprachwahl. Alle bleiben mit
        // Absicht deutsch: die Oberflaeche ist englisch, der Betreiber
        // denkt deutsch, und beide Wege sollen ins Ziel fuehren. Sie
        // stehen neben ihren englischen Entsprechungen.
        //
        // Jede weitere Uebersetzung senkt die Zahl; wer eine neue
        // deutsche Beschriftung einbaut, laeuft auf.
        constexpr int kDecke = 9;
        QVERIFY2(funde.size() <= kDecke,
                 qPrintable(QStringLiteral(
                     "%1 deutsche Texte in src/gui — die Decke steht bei "
                     "%2. „englisch durchgehend\": neue deutsche "
                     "Beschriftungen gehoeren durch tr() ins Englische, "
                     "die Uebersetzung in translations/longpath_de.ts.")
                                .arg(funde.size()).arg(kDecke)));
    }
};

QTEST_MAIN(TstOberflaecheEnglisch)
#include "tst_oberflaeche_englisch.moc"
