// Die Tonleuchte im VFO-Flag.
//
// Sie ist das einzige, was Martin vom Tonsquelch sieht: leuchtet sie,
// liegt der eingestellte Subton an; ist sie dunkel, fehlt er und der
// Kanal ist stumm. Geprueft wird hier, dass sie das Richtige zeigt --
// und vor allem, dass sie NICHT da ist, wo gar niemand auf einen Ton
// hoert. Eine dunkle Leuchte bei "Off" oder "Encode" waere eine
// Behauptung ueber etwas, das nicht laeuft, und im Betrieb genau die
// Art Anzeige, auf die man sich nicht mehr verlaesst.
//
// Longpath-eigen, kein Thetis-Port: Thetis hat keinen Tondetektor und
// darum auch nichts anzuzeigen.

#include <QLabel>
#include <QComboBox>
#include <QtTest>

#include "gui/StyleConstants.h"
#include "gui/widgets/VfoModeContainers.h"
#include "models/SliceModel.h"

using namespace Longpath;

namespace {

template<typename T>
T* findNamed(QWidget* parent, const char* name)
{
    return parent->findChild<T*>(QString::fromLatin1(name));
}

/// Enthaelt das Stilblatt der Leuchte diese Farbe?
bool leuchtetIn(QLabel* lamp, const char* farbe)
{
    return lamp && lamp->styleSheet().contains(QLatin1String(farbe), Qt::CaseInsensitive);
}

} // namespace

class TstCtcssTonleuchte : public QObject
{
    Q_OBJECT

private slots:
    void dieLeuchteGibtEsUeberhaupt();
    void sieIstNurBeiDecodeUndEncDecSichtbar();
    void sieLeuchtetWennDerTonAnliegt();
    void sieIstDunkelWennDerTonFehlt();
    void sieIstNichtRot();
    void sieFolgtDerMeldungOhneUmweg();
    void derHinweisNenntDenEingestelltenTon();
};

void TstCtcssTonleuchte::dieLeuchteGibtEsUeberhaupt()
{
    SliceModel s;
    FmOptContainer c;
    c.setSlice(&s);
    c.syncFromSlice();
    QVERIFY2(findNamed<QLabel>(&c, "toneLamp") != nullptr,
             "Die Tonleuchte fehlt im FM-Flag");
}

void TstCtcssTonleuchte::sieIstNurBeiDecodeUndEncDecSichtbar()
{
    SliceModel s;
    FmOptContainer c;
    c.setSlice(&s);
    // Sichtbarkeit laesst sich nur an einem gezeigten Fenster ablesen;
    // offscreen ist das in Ordnung (Martins Regel: Pruefprogramme nur
    // offscreen).
    c.show();
    QVERIFY(QTest::qWaitForWindowExposed(&c));

    auto* lamp = findNamed<QLabel>(&c, "toneLamp");
    QVERIFY(lamp != nullptr);

    struct Fall { int modus; bool sichtbar; const char* name; };
    const Fall faelle[] = {
        {0, false, "Off"},
        {1, false, "CTCSS Encode"},
        {2, true,  "CTCSS Decode"},
        {3, true,  "CTCSS Enc+Dec"},
    };
    for (const Fall& f : faelle) {
        s.setFmCtcssMode(f.modus);
        c.syncFromSlice();
        QVERIFY2(lamp->isVisible() == f.sichtbar,
                 qPrintable(QStringLiteral("Modus %1 (%2): Leuchte %3, erwartet %4")
                                .arg(f.modus).arg(QLatin1String(f.name))
                                .arg(lamp->isVisible() ? "sichtbar" : "verborgen")
                                .arg(f.sichtbar ? "sichtbar" : "verborgen")));
    }
}

void TstCtcssTonleuchte::sieLeuchtetWennDerTonAnliegt()
{
    SliceModel s;
    s.setFmCtcssMode(2);              // Decode
    s.setFmCtcssToneDetected(true);
    FmOptContainer c;
    c.setSlice(&s);
    c.syncFromSlice();

    auto* lamp = findNamed<QLabel>(&c, "toneLamp");
    QVERIFY(lamp != nullptr);
    QVERIFY2(leuchtetIn(lamp, Style::kLiveGreen),
             qPrintable(QStringLiteral("Bei anliegendem Ton nicht kLiveGreen: %1")
                            .arg(lamp->styleSheet())));
}

void TstCtcssTonleuchte::sieIstDunkelWennDerTonFehlt()
{
    SliceModel s;
    s.setFmCtcssMode(2);
    s.setFmCtcssToneDetected(false);
    FmOptContainer c;
    c.setSlice(&s);
    c.syncFromSlice();

    auto* lamp = findNamed<QLabel>(&c, "toneLamp");
    QVERIFY(lamp != nullptr);
    QVERIFY2(leuchtetIn(lamp, Style::kBorderMuted),
             qPrintable(QStringLiteral("Bei fehlendem Ton nicht kBorderMuted: %1")
                            .arg(lamp->styleSheet())));
    QVERIFY2(!leuchtetIn(lamp, Style::kLiveGreen),
             "Bei fehlendem Ton leuchtet sie trotzdem gruen");
}

void TstCtcssTonleuchte::sieIstNichtRot()
{
    // Martins Regel: kein kraeftiges Rot als Zustandsfarbe, Rot bleibt
    // der Warnung. "Gerade kein Ton" ist der Normalfall zwischen zwei
    // Durchgaengen und keine Warnung.
    SliceModel s;
    s.setFmCtcssMode(2);
    FmOptContainer c;
    c.setSlice(&s);

    for (bool tonDa : {true, false}) {
        s.setFmCtcssToneDetected(tonDa);
        c.syncFromSlice();
        auto* lamp = findNamed<QLabel>(&c, "toneLamp");
        QVERIFY(lamp != nullptr);
        for (const char* rot : {Style::kTxRed, Style::kRedBg, Style::kRedText}) {
            QVERIFY2(!leuchtetIn(lamp, rot),
                     qPrintable(QStringLiteral("Die Leuchte benutzt Rot (%1) bei "
                                               "tonDa=%2: %3")
                                    .arg(QLatin1String(rot)).arg(tonDa)
                                    .arg(lamp->styleSheet())));
        }
    }
}

void TstCtcssTonleuchte::sieFolgtDerMeldungOhneUmweg()
{
    // Der Weg, auf dem es im Betrieb ankommt: RadioModel schreibt die
    // Meldung des Detektors in den Slice, und die Leuchte muss von
    // selbst folgen -- ohne dass jemand syncFromSlice() ruft.
    SliceModel s;
    s.setFmCtcssMode(2);
    FmOptContainer c;
    c.setSlice(&s);
    c.syncFromSlice();

    auto* lamp = findNamed<QLabel>(&c, "toneLamp");
    QVERIFY(lamp != nullptr);
    QVERIFY2(leuchtetIn(lamp, Style::kBorderMuted), "Startzustand ist nicht dunkel");

    s.setFmCtcssToneDetected(true);   // KEIN syncFromSlice danach
    QVERIFY2(leuchtetIn(lamp, Style::kLiveGreen),
             "Die Leuchte folgte der Meldung nicht von selbst");

    s.setFmCtcssToneDetected(false);
    QVERIFY2(leuchtetIn(lamp, Style::kBorderMuted),
             "Die Leuchte ging nach dem Tonende nicht aus");
}

void TstCtcssTonleuchte::derHinweisNenntDenEingestelltenTon()
{
    SliceModel s;
    s.setFmCtcssMode(2);
    s.setFmCtcssValueHz(123.0);
    s.setFmCtcssToneDetected(false);
    FmOptContainer c;
    c.setSlice(&s);
    c.syncFromSlice();

    auto* lamp = findNamed<QLabel>(&c, "toneLamp");
    QVERIFY(lamp != nullptr);
    // Beim Warten muss dastehen, worauf gewartet wird -- und dass der
    // Kanal deswegen stumm ist. Ohne das sucht man den Fehler beim
    // Lautsprecher.
    QVERIFY2(lamp->toolTip().contains(QStringLiteral("123.0")),
             qPrintable(QStringLiteral("Der Hinweis nennt den Ton nicht: %1")
                            .arg(lamp->toolTip())));
    QVERIFY2(lamp->toolTip().contains(QStringLiteral("stumm")),
             qPrintable(QStringLiteral("Der Hinweis sagt nicht, dass der Kanal stumm "
                                       "ist: %1").arg(lamp->toolTip())));

    s.setFmCtcssToneDetected(true);
    QVERIFY2(lamp->toolTip().contains(QStringLiteral("123.0")),
             qPrintable(QStringLiteral("Der Hinweis nennt den Ton nicht: %1")
                            .arg(lamp->toolTip())));
}

QTEST_MAIN(TstCtcssTonleuchte)
#include "tst_ctcss_tonleuchte.moc"
