// Prüfstand: gleichzeitige Logzeilen dürfen sich nicht zerschreiben.
//
// Warum es das gibt: am 2026-10-03 standen in Martins Betriebslogs sechs
// zerschriebene Zeilen, alle mit derselben Handschrift — vorne abgeschnitten,
// hinten ganz:
//
//     TERACTIVE QoS                                    (2x)
//     io() — sending discovery broadcast (bench-...)   (2x)
//     27] INF: DSP thread joined default output workgroup
//     rom MAC: "..." (override via PGXL_FlexRadioSerial key)
//
// Ursache: `main.cpp` legte je Meldung einen eigenen QTextStream auf DIESELBE
// QFile. qInstallMessageHandler ruft aus jedem Faden, Longpath hat ihrer
// mindestens fünf — zwei gleichzeitige Meldungen landen übereinander.
//
// Zwei davon waren ausgerechnet die QoS-Meldung, also genau die Zeile, der an
// dem Tag jemand nachgegangen ist. Eine zerschriebene Zeile ist schlimmer als
// eine fehlende: die fehlende vermisst man, die zerschriebene deutet man.
//
// Geprüft wird darum das, was die Oberfläche nie prüfen kann: ob nach vielen
// Fäden JEDE Zeile vollständig und genau einmal dasteht.

#include <QtTest>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>

#include "core/LogDatei.h"

namespace {

constexpr int kFaeden = 8;
constexpr int kZeilenJeFaden = 250;

/** Ein Faden, der seine Zeilen so schnell wie möglich hinausschreibt. */
class Schreiber : public QThread {
public:
    explicit Schreiber(int nummer) : m_nummer(nummer) {}

    void run() override
    {
        for (int i = 0; i < kZeilenJeFaden; ++i) {
            // Unterschiedliche Längen, damit eine Überlagerung auffällt: bei
            // gleich langen Zeilen könnte eine Zerstörung zufällig wieder wie
            // eine gültige Zeile aussehen.
            Longpath::Log::schreibe(
                QStringLiteral("[F%1] %2 %3\n")
                    .arg(m_nummer)
                    .arg(i)
                    .arg(QString(3 + (i % 37), QLatin1Char('x'))));
        }
    }

private:
    int m_nummer;
};

}  // namespace

class TestLogSchreibtAtomar : public QObject {
    Q_OBJECT

private slots:

    /** Acht Fäden, 250 Zeilen je Faden: hinterher müssen genau 2000
     *  vollständige, verschiedene Zeilen dastehen. */
    void gleichzeitigeZeilenBleibenGanz()
    {
        QTemporaryDir ordner;
        QVERIFY(ordner.isValid());
        const QString pfad = ordner.path() + QStringLiteral("/longpath-pruef.log");

        QFile datei(pfad);
        QVERIFY2(datei.open(QIODevice::WriteOnly | QIODevice::Truncate
                            | QIODevice::Text),
                 "Prüfdatei liess sich nicht oeffnen");
        Longpath::Log::setzeDatei(&datei);

        std::vector<std::unique_ptr<Schreiber>> faeden;
        faeden.reserve(kFaeden);
        for (int f = 0; f < kFaeden; ++f) {
            faeden.push_back(std::make_unique<Schreiber>(f));
        }
        for (auto& t : faeden) { t->start(); }
        for (auto& t : faeden) { QVERIFY(t->wait(60000)); }

        // Abmelden VOR dem Schliessen — dieselbe Reihenfolge wie in main().
        Longpath::Log::setzeDatei(nullptr);
        datei.close();

        QVERIFY(datei.open(QIODevice::ReadOnly | QIODevice::Text));
        const QStringList zeilen =
            QString::fromUtf8(datei.readAll()).split(QLatin1Char('\n'),
                                                     Qt::SkipEmptyParts);
        datei.close();

        QCOMPARE(zeilen.size(), kFaeden * kZeilenJeFaden);

        // Jede Zeile muss vollständig sein: Kopf [Fn], eine Nummer, und
        // danach NUR x-e. Ein zerschriebenes Paar faellt hier durch, weil der
        // Rest der einen Zeile hinter dem Anfang der anderen steht.
        const QRegularExpression form(
            QStringLiteral("^\\[F[0-7]\\] \\d+ x+$"));
        QSet<QString> gesehen;
        for (const QString& z : zeilen) {
            QVERIFY2(form.match(z).hasMatch(),
                     qPrintable(QStringLiteral("Zerschriebene Zeile: ") + z));
            QVERIFY2(!gesehen.contains(z),
                     qPrintable(QStringLiteral("Zeile doppelt: ") + z));
            gesehen.insert(z);
        }
        QCOMPARE(gesehen.size(), kFaeden * kZeilenJeFaden);
    }

    /** Ohne gesetzte Datei darf nichts abstuerzen — der Weg vor dem Oeffnen
     *  des Logs und nach dem Schliessen. */
    void ohneDateiNurBildschirm()
    {
        Longpath::Log::setzeDatei(nullptr);
        Longpath::Log::schreibe(QStringLiteral("ohne Datei\n"));
        QVERIFY(true);   // kein Absturz ist hier die ganze Zusage
    }
};

QTEST_MAIN(TestLogSchreibtAtomar)
#include "tst_log_schreibt_atomar.moc"
