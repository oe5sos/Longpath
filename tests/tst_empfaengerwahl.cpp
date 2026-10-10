// Jeder Empfaenger bekommt eine Aufgabe: die Empfaengerwahl der Applets.
//
// Bis hierher hingen die Decoder am AKTIVEN Empfaenger
// (`RadioModel::activeSlice()`). Wer den Empfaenger wechselte, nahm den
// Decoder mit -- RX1 auf Sprache lassen und RX2 nebenher CW dekodieren
// ging nicht. Das ist Punkt 2 der Zeus-Liste.
//
// Drei Faelle hier sind gegen Fehler gebaut, die beim Bauen wirklich
// passiert sind:
//
//   * `derZusatzStehtGleichNachDemStartInDerLeiste`: `buildUI()` laeuft
//     bei CwDecoderApplet als ERSTES im Konstruktor, also vor
//     `initReceiverBinding()`. Setzte nur `appletTitleBar()` den Zusatz,
//     stuende nach dem Start "CW DECODER" da, obwohl ein fester
//     Empfaenger gewaehlt ist -- und der Zusatz kaeme erst beim
//     naechsten Wechsel.
//   * `einWechselZiehtDasFesteAppletNichtMit`: der eigentliche Zweck.
//   * `eineWahlAufEinenFehlendenEmpfaengerIstKeinAbsturz`: ein
//     Empfaenger kann verschwinden, waehrend ein Applet auf ihn zeigt.

#include <QComboBox>
#include <QLabel>
#include <QSignalSpy>
#include <QtTest>

#include "core/AppSettings.h"
#include "gui/applets/AppletWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

namespace {

/// Ein Applet, das nur das Noetigste tut -- die Bindung steckt in der
/// Basisklasse, und genau die wird hier geprueft.
class ProbeApplet : public AppletWidget {
public:
    explicit ProbeApplet(RadioModel* model, QString id, QWidget* parent = nullptr)
        : AppletWidget(model, parent), m_id(std::move(id))
    {
        // Wie CwDecoderApplet: die Oberflaeche zuerst, die Bindung danach.
        m_leiste = appletTitleBar(QStringLiteral("PROBE"));
        initReceiverBinding();
        connect(this, &AppletWidget::boundSliceChanged, this,
                [this](SliceModel* s) { m_zuletzt = s; ++m_meldungen; });
    }

    QString appletId() const override { return m_id; }
    QString appletTitle() const override { return QStringLiteral("Probe"); }
    void    syncFromModel() override {}

    /// Was in der Titelleiste steht.
    QString leistenText() const
    {
        auto* l = m_leiste ? m_leiste->findChild<QLabel*>() : nullptr;
        // Der erste QLabel ist der Griff, der zweite der Titel.
        const QList<QLabel*> alle = m_leiste ? m_leiste->findChildren<QLabel*>()
                                             : QList<QLabel*>{};
        Q_UNUSED(l);
        return alle.size() >= 2 ? alle.at(1)->text() : QString();
    }

    SliceModel* zuletztGemeldet() const { return m_zuletzt; }
    int         meldungen() const { return m_meldungen; }
    void        zaehlerZuruecksetzen() { m_meldungen = 0; }

    using AppletWidget::receiverChoiceWidget;

private:
    QString m_id;
    QWidget* m_leiste{nullptr};
    SliceModel* m_zuletzt{nullptr};
    int m_meldungen{0};
};

} // namespace

class TstEmpfaengerwahl : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void dieVorgabeIstFolgtDemAktiven();
    void beimFolgenWandertDieBindungMit();
    void einWechselZiehtDasFesteAppletNichtMit();
    void derZusatzStehtGleichNachDemStartInDerLeiste();
    void dieWahlWirdGemerkt();
    void eineWahlAufEinenFehlendenEmpfaengerIstKeinAbsturz();
    void derWaehlerBietetJedenEmpfaengerAn();
    void einSpaeterDazukommenderEmpfaengerWirdNachgebunden();
    void dieWahlHaengtAmGeraetNichtAmListenplatz();
};

void TstEmpfaengerwahl::init()
{
    // Jeder Fall faengt mit leerer Erinnerung an -- sonst traegt die
    // Wahl aus dem vorigen Fall herueber.
    AppSettings::instance().remove(QStringLiteral("Applet/probe/Receiver"));
    AppSettings::instance().remove(QStringLiteral("Applet/probe2/Receiver"));
}

void TstEmpfaengerwahl::dieVorgabeIstFolgtDemAktiven()
{
    RadioModel model;
    ProbeApplet applet(&model, QStringLiteral("probe"));
    QCOMPARE(applet.receiverChoice(), -1);
    // Keine Wahl heisst kein Zusatz: "PROBE", nicht "PROBE · RX1".
    QCOMPARE(applet.leistenText(), QStringLiteral("PROBE"));
}

void TstEmpfaengerwahl::beimFolgenWandertDieBindungMit()
{
    RadioModel model;
    ProbeApplet applet(&model, QStringLiteral("probe"));
    QCOMPARE(applet.receiverChoice(), -1);
    // Ohne Empfaenger im Modell gibt es nichts zu binden -- das ist kein
    // Fehler, sondern der Zustand vor dem Verbinden.
    QCOMPARE(applet.boundSlice(), model.activeSlice());
}

void TstEmpfaengerwahl::einWechselZiehtDasFesteAppletNichtMit()
{
    // Der eigentliche Zweck des ganzen Punktes: ein fest gewaehltes
    // Applet bleibt an SEINEM Empfaenger, auch wenn woanders gearbeitet
    // wird.
    //
    // Dieser Fall braucht echte Empfaenger im Modell. Mit einem leeren
    // Modell faellt `activeSlice()` ohnehin auf nullptr, und dann sieht
    // ein Applet, das faelschlich dem Aktiven folgt, genauso aus wie
    // eines, das richtig bindet -- der Fall pruefte dann nichts.
    RadioModel model;
    model.addSlice();
    model.addSlice();
    const QList<SliceModel*> empfaenger = model.slices();
    QVERIFY2(empfaenger.size() >= 2, "Das Modell gab keine zwei Empfaenger her");

    ProbeApplet folgt(&model, QStringLiteral("probe"));
    ProbeApplet fest(&model, QStringLiteral("probe2"));
    fest.setReceiverChoice(empfaenger.at(1)->sliceIndex());   // RX2, als Kennung

    model.setActiveSlice(0);
    QCOMPARE(folgt.boundSlice(), empfaenger.at(0));
    QCOMPARE(fest.boundSlice(),  empfaenger.at(1));

    // Jetzt der Wechsel: das folgende Applet geht mit, das feste nicht.
    folgt.zaehlerZuruecksetzen();
    fest.zaehlerZuruecksetzen();
    model.setActiveSlice(1);

    QCOMPARE(folgt.boundSlice(), empfaenger.at(1));
    QVERIFY2(fest.boundSlice() == empfaenger.at(1),
             "Das feste Applet zeigt nicht mehr auf RX2");
    // Und zurueck -- jetzt trennt sich die Spreu.
    model.setActiveSlice(0);
    QCOMPARE(folgt.boundSlice(), empfaenger.at(0));
    QVERIFY2(fest.boundSlice() == empfaenger.at(1),
             "Der Wechsel hat das feste Applet doch mitgezogen");

    // Das feste Applet darf ueber die ganzen Wechsel keine einzige
    // Bindungsaenderung gemeldet haben -- es hat sich ja nichts
    // geaendert.
    QVERIFY2(fest.meldungen() == 0,
             qPrintable(QStringLiteral("Das feste Applet meldete %1 Bindungswechsel")
                            .arg(fest.meldungen())));
    QVERIFY2(folgt.meldungen() > 0, "Das folgende Applet meldete keinen Wechsel");
}

void TstEmpfaengerwahl::derZusatzStehtGleichNachDemStartInDerLeiste()
{
    // `buildUI()` laeuft bei den echten Applets vor
    // `initReceiverBinding()`. Setzte nur `appletTitleBar()` den Zusatz,
    // fehlte er nach dem Start.
    RadioModel model;
    model.addSlice();
    const int idB = model.addSlice();
    AppSettings::instance().setValue(QStringLiteral("Applet/probe/Receiver"), idB);

    ProbeApplet applet(&model, QStringLiteral("probe"));
    QCOMPARE(applet.receiverChoice(), idB);
    QVERIFY2(applet.leistenText() == QStringLiteral("PROBE · RX2"),
             qPrintable(QStringLiteral("In der Leiste steht \"%1\", erwartet "
                                       "\"PROBE · RX2\"").arg(applet.leistenText())));
}

void TstEmpfaengerwahl::dieWahlWirdGemerkt()
{
    int idB = -1;
    {
        RadioModel model;
        model.addSlice();
        idB = model.addSlice();
        ProbeApplet applet(&model, QStringLiteral("probe"));
        applet.setReceiverChoice(idB);
    }
    // Neues Applet, dieselbe Applet-Kennung: die Wahl muss wieder da sein.
    RadioModel model2;
    model2.addSlice();
    model2.addSlice();
    ProbeApplet wieder(&model2, QStringLiteral("probe"));
    QCOMPARE(wieder.receiverChoice(), idB);
    QCOMPARE(wieder.leistenText(), QStringLiteral("PROBE · RX2"));

    // Und eine andere Kennung teilt die Wahl NICHT.
    ProbeApplet anderes(&model2, QStringLiteral("probe2"));
    QCOMPARE(anderes.receiverChoice(), -1);
}

void TstEmpfaengerwahl::eineWahlAufEinenFehlendenEmpfaengerIstKeinAbsturz()
{
    RadioModel model;
    ProbeApplet applet(&model, QStringLiteral("probe"));
    applet.setReceiverChoice(77);                // gibt es sicher nicht
    QCOMPARE(applet.boundSlice(), nullptr);
    QCOMPARE(applet.leistenText(), QStringLiteral("PROBE · RX? (fehlt)"));
    // Und der Waehler muss die fehlende Wahl trotzdem zeigen, sonst
    // faende man nicht heraus, worauf das Applet wartet.
    QWidget halter;
    QWidget* w = applet.receiverChoiceWidget(&halter);
    QVERIFY(w != nullptr);
    auto* cmb = w->findChild<QComboBox*>(QStringLiteral("receiverChoiceCmb"));
    QVERIFY(cmb != nullptr);
    QVERIFY2(cmb->findData(77) >= 0, "Die Wahl auf einen fehlenden Empfaenger fehlt im Waehler");
    QVERIFY2(cmb->currentData().toInt() == 77, "Der Waehler zeigt nicht die geltende Wahl");
}

void TstEmpfaengerwahl::derWaehlerBietetJedenEmpfaengerAn()
{
    RadioModel model;
    ProbeApplet applet(&model, QStringLiteral("probe"));
    QWidget halter;
    QWidget* w = applet.receiverChoiceWidget(&halter);
    QVERIFY(w != nullptr);
    auto* cmb = w->findChild<QComboBox*>(QStringLiteral("receiverChoiceCmb"));
    QVERIFY(cmb != nullptr);
    // Mindestens "folgt dem aktiven" muss drinstehen, und es muss die
    // Vorgabe sein.
    QVERIFY2(cmb->findData(-1) >= 0, "\"folgt dem aktiven\" fehlt im Waehler");
    QCOMPARE(cmb->currentData().toInt(), -1);
    // Je Empfaenger im Modell ein Eintrag, dazu der Folge-Eintrag.
    QCOMPARE(cmb->count(), static_cast<int>(model.slices().size()) + 1);
}

void TstEmpfaengerwahl::einSpaeterDazukommenderEmpfaengerWirdNachgebunden()
{
    // Wer RX2 waehlt, bevor es RX2 gibt, zeigt zunaechst auf nichts.
    // Kommt der Empfaenger spaeter dazu, muss die Bindung nachziehen --
    // sonst bleibt das Applet stumm, obwohl sein Empfaenger da ist, und
    // man sucht den Fehler im Decoder.
    RadioModel model;
    model.addSlice();                            // nur RX1
    ProbeApplet applet(&model, QStringLiteral("probe"));
    // Die Kennung, die der zweite Empfaenger bekommen WIRD (addSlice gibt
    // die kleinste freie aus, der erste hat 0).
    applet.setReceiverChoice(1);
    QCOMPARE(applet.boundSlice(), nullptr);

    applet.zaehlerZuruecksetzen();
    model.addSlice();                            // jetzt kommt RX2
    const QList<SliceModel*> empfaenger = model.slices();
    QVERIFY2(empfaenger.size() >= 2, "Das Modell gab keinen zweiten Empfaenger her");

    QVERIFY2(applet.boundSlice() == empfaenger.at(1),
             "Der neue Empfaenger wurde nicht nachgebunden");
    QVERIFY2(applet.meldungen() > 0,
             "Das Nachbinden wurde nicht gemeldet -- der Decoder erfaehrt nichts davon");
}

void TstEmpfaengerwahl::dieWahlHaengtAmGeraetNichtAmListenplatz()
{
    // Der Fall, der den Entwurf gedreht hat.
    //
    // Erst war die Wahl eine LISTENPOSITION: "RX2" war der zweite
    // Empfaenger in der Liste. Loescht man dann RX1, ruecken die
    // Ueberlebenden nach -- und das Applet haengt mit derselben Position
    // ploetzlich an einem ANDEREN Geraet. Im Betrieb waere das ein
    // Fehler, den niemand sucht: der Decoder laeuft weiter und zeigt
    // Text, nur vom falschen Empfaenger.
    //
    // Darum ist die Wahl eine KENNUNG (`RadioModel::sliceById`: "stable
    // for the life of a slice"). Die Beschriftung "RX2" rechnet sich aus
    // der Position und darf ruhig zu "RX1" werden -- der Empfaenger
    // dahinter bleibt derselbe.
    RadioModel model;
    const int idA = model.addSlice();
    const int idB = model.addSlice();
    SliceModel* b = model.sliceById(idB);
    QVERIFY(b != nullptr);

    ProbeApplet applet(&model, QStringLiteral("probe"));
    applet.setReceiverChoice(idB);
    QCOMPARE(applet.boundSlice(), b);
    QCOMPARE(applet.leistenText(), QStringLiteral("PROBE · RX2"));

    // Jetzt faellt der ERSTE weg. Der gewaehlte rueckt auf Platz 1.
    model.removeSlice(idA);

    QVERIFY2(applet.boundSlice() == b,
             "Nach dem Loeschen von RX1 haengt das Applet an einem anderen Geraet");
    QCOMPARE(applet.receiverChoice(), idB);
    // Die Beschriftung zieht mit -- derselbe Empfaenger heisst jetzt RX1.
    QVERIFY2(applet.leistenText() == QStringLiteral("PROBE · RX1"),
             qPrintable(QStringLiteral("In der Leiste steht \"%1\", erwartet "
                                       "\"PROBE · RX1\"").arg(applet.leistenText())));
}

QTEST_MAIN(TstEmpfaengerwahl)
#include "tst_empfaengerwahl.moc"
