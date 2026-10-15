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

#include <QtTest>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

namespace {

/// Konservativ: nur Woerter, die im Englischen nicht vorkommen. Ein
/// falscher Treffer kostet eine Uebersetzung, die niemand braucht;
/// lieber ein paar durchlassen als die Liste unbrauchbar machen.
const QStringList& deutscheWoerter()
{
    static const QStringList w = {
        QStringLiteral("der"),    QStringLiteral("die"),
        QStringLiteral("das"),    QStringLiteral("und"),
        QStringLiteral("nicht"),  QStringLiteral("kein"),
        QStringLiteral("keine"),  QStringLiteral("eine"),
        QStringLiteral("einen"),  QStringLiteral("ist"),
        QStringLiteral("sind"),   QStringLiteral("wird"),
        QStringLiteral("werden"), QStringLiteral("schon"),
        QStringLiteral("noch"),   QStringLiteral("beim"),
        QStringLiteral("Fenster"),      QStringLiteral("Hintergrund"),
        QStringLiteral("Einstellungen"),QStringLiteral("Abbrechen"),
        QStringLiteral("Speichern"),    QStringLiteral("Sichern"),
        QStringLiteral("Entfernen"),    QStringLiteral("Umbenennen"),
        QStringLiteral("Duplizieren"),  QStringLiteral("Leistung"),
        QStringLiteral("Senden"),       QStringLiteral("Empfang"),
        QStringLiteral("Frequenz"),     QStringLiteral("Anzeige"),
        QStringLiteral("Auswahl"),      QStringLiteral("Fehler"),
        QStringLiteral("Profil"),       QStringLiteral("Datei"),
        QStringLiteral("Bild"),         QStringLiteral("Farbe"),
        // Nachgetragen am 2026-10-15, nachdem der vollstaendige
        // ctest-Lauf 31 weitere Texte zutage brachte, die diese Liste
        // nicht kannte — siehe die Warnung im Kopf.
        QStringLiteral("bearbeiten"),   QStringLiteral("trennen"),
        QStringLiteral("verbinden"),    QStringLiteral("Verbindung"),
        QStringLiteral("Seitenbereich"),QStringLiteral("Knopfleiste"),
        QStringLiteral("Statuszeile"),  QStringLiteral("Logbuch"),
        QStringLiteral("Kanalzug"),     QStringLiteral("Zentrale"),
        QStringLiteral("Scheibe"),      QStringLiteral("Schiene"),
        QStringLiteral("Breite"),       QStringLiteral("Hoehe"),
        QStringLiteral("Groesse"),      QStringLiteral("Wert"),
        QStringLiteral("Eintrag"),      QStringLiteral("Leiste"),
        QStringLiteral("Zeile"),        QStringLiteral("Spektrum"),
        QStringLiteral("Wasserfall"),   QStringLiteral("Stehwelle"),
        QStringLiteral("Mitschrift"),   QStringLiteral("Abstimmen"),
        QStringLiteral("Betriebsart"),  QStringLiteral("Messanzeigen"),
        QStringLiteral("anzeigen"),     QStringLiteral("zeigen"),
        QStringLiteral("Alle"),         QStringLiteral("Fertig"),
        QStringLiteral("festen"),       QStringLiteral("Anlegen"),
    };
    return w;
}

bool riechtDeutsch(const QString& t)
{
    for (const QChar c : t) {
        if (c == QChar(u'ä') || c == QChar(u'ö') || c == QChar(u'ü')
            || c == QChar(u'Ä') || c == QChar(u'Ö') || c == QChar(u'Ü')
            || c == QChar(u'ß')) {
            return true;
        }
    }
    static const QRegularExpression wortgrenze(
        QStringLiteral("[^A-Za-zÄÖÜäöüß]"));
    const QStringList teile =
        t.split(wortgrenze, Qt::SkipEmptyParts);
    for (const QString& teil : teile) {
        for (const QString& w : deutscheWoerter()) {
            if (teil.compare(w, Qt::CaseInsensitive) == 0) { return true; }
        }
    }
    return false;
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
        static const QRegularExpression sichtbar(
            QStringLiteral("(?:\\btr|QStringLiteral|QLatin1String)"
                           "\\(\\s*\"((?:[^\"\\\\]|\\\\.)*)\""));

        QStringList funde;
        QDirIterator it(wurzel, {QStringLiteral("*.cpp"), QStringLiteral("*.h")},
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString pfad = it.next();
            QFile f(pfad);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { continue; }
            QTextStream in(&f);
            int zeile = 0;
            while (!in.atEnd()) {
                const QString z = in.readLine();
                ++zeile;
                const QString roh = z.trimmed();
                // Kommentare sind deutsch, mit Absicht.
                if (roh.startsWith(QLatin1String("//"))
                    || roh.startsWith(QLatin1Char('*'))) {
                    continue;
                }
                // Protokollzeilen auch. Die liest der Betreiber, nicht
                // der Benutzer — dieselbe Regel wie fuer src/core, nur
                // dass sie hier mitten in einer GUI-Datei stehen.
                if (roh.startsWith(QLatin1String("qC"))
                    || roh.startsWith(QLatin1String("qDebug"))
                    || roh.startsWith(QLatin1String("qInfo"))
                    || roh.startsWith(QLatin1String("qWarning"))
                    || roh.startsWith(QLatin1String("<<"))) {
                    continue;
                }
                auto m = sichtbar.globalMatch(z);
                while (m.hasNext()) {
                    const QString t = m.next().captured(1);
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
        // Die sechs, die bleiben, sind SCHLAGWOERTER der Widget-Suche
        // („frequenz", „abstimmen", „stehwelle", „empfang", „senden",
        // „leistung") und bleiben mit
        // Absicht deutsch: die Oberflaeche ist englisch, der Betreiber
        // denkt deutsch, und beide Wege sollen ins Ziel fuehren. Sie
        // stehen neben ihren englischen Entsprechungen.
        //
        // Jede weitere Uebersetzung senkt die Zahl; wer eine neue
        // deutsche Beschriftung einbaut, laeuft auf.
        constexpr int kDecke = 6;
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
