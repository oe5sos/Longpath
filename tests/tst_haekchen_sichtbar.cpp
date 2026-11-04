// tests/tst_haekchen_sichtbar.cpp  (Longpath)
//
// Betreiber am 2026-11-04, am Update-Dialog: "man sieht nicht, ob das
// Haekchen an ist oder nicht. das sollte ueberall kontrolliert werden."
//
// Der Hausstil (Style::kCheckBoxStyle, kRadioButtonStyle) zeichnete "an"
// als einen etwas helleren Rand auf demselben schwarzen Grund -- auf
// einem dunklen Schirm nicht vom "aus" zu unterscheiden. Dieser
// Pruefstand misst, nicht meint: er zeichnet ein Haekchen und einen
// Rundknopf je einmal aus und an, nimmt die Helligkeit innerhalb des
// Indikators (die ersten 14x14 Punkte) und verlangt einen deutlichen
// Unterschied. Alte Fassung: der Unterschied liegt bei wenigen Stufen,
// beide Faelle fallen um.
//
// Gemessen wird an den Stil-Konstanten selbst, nicht an einem einzelnen
// Dialog: jeder Dialog, der sie uebernimmt (Setup-Seiten, TX-EQ, TCI-
// Protokoll, der Update-Dialog ueber das Hauptfenster), bekommt dasselbe.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-04 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

// no-port-check: Longpath-original test file.

#include <QtTest>
#include <QCheckBox>
#include <QRadioButton>
#include <QImage>

#include "gui/StyleConstants.h"
#include "FensterFrist.h"

using namespace Longpath;

namespace {

// Mittlere Helligkeit (0..255) im Indikator-Feld links im Widget.
double helligkeitImIndikator(QWidget* w)
{
    const QImage img = w->grab().toImage().convertToFormat(QImage::Format_RGB32);
    // Der Indikator sitzt links, vertikal mittig; 14x14 Punkte auf
    // einem Retina-Schirm doppelt so viele -- darum ueber den
    // Geraetemassstab.
    const qreal s = img.devicePixelRatio() > 0 ? img.devicePixelRatio() : 1.0;
    const int groesse = int(14 * s);
    const int oben = (img.height() - groesse) / 2;
    double summe = 0.0;
    int n = 0;
    for (int y = oben; y < oben + groesse && y < img.height(); ++y) {
        for (int x = 0; x < groesse && x < img.width(); ++x) {
            summe += qGray(img.pixel(x, y));
            ++n;
        }
    }
    return n > 0 ? summe / n : 0.0;
}

} // namespace

class TestHaekchenSichtbar : public QObject
{
    Q_OBJECT

private slots:
    void einHaekchenAnUnterscheidetSichVonAus()
    {
        QWidget traeger;
        traeger.setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
        auto* box = new QCheckBox(QStringLiteral("Check for updates when Longpath starts"), &traeger);
        box->move(0, 0);
        traeger.resize(320, 24);
        traeger.show();
        QVERIFY(QTest::qWaitForWindowExposed(&traeger, kFensterFrist));

        box->setChecked(false);
        const double aus = helligkeitImIndikator(box);
        box->setChecked(true);
        const double an = helligkeitImIndikator(box);
        QVERIFY2(an - aus >= 25.0,
                 qPrintable(QStringLiteral("Haekchen: aus %1, an %2 -- kein sichtbarer Unterschied")
                                .arg(aus, 0, 'f', 1).arg(an, 0, 'f', 1)));
    }

    void einRundknopfAnUnterscheidetSichVonAus()
    {
        QWidget traeger;
        traeger.setStyleSheet(QString::fromLatin1(Style::kRadioButtonStyle));
        auto* knopf = new QRadioButton(QStringLiteral("Option"), &traeger);
        knopf->move(0, 0);
        traeger.resize(200, 24);
        traeger.show();
        QVERIFY(QTest::qWaitForWindowExposed(&traeger, kFensterFrist));

        knopf->setChecked(false);
        const double aus = helligkeitImIndikator(knopf);
        knopf->setChecked(true);
        const double an = helligkeitImIndikator(knopf);
        QVERIFY2(an - aus >= 20.0,
                 qPrintable(QStringLiteral("Rundknopf: aus %1, an %2 -- kein sichtbarer Unterschied")
                                .arg(aus, 0, 'f', 1).arg(an, 0, 'f', 1)));
    }
};

QTEST_MAIN(TestHaekchenSichtbar)
#include "tst_haekchen_sichtbar.moc"
