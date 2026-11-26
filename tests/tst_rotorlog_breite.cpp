// SPDX-License-Identifier: GPL-3.0-or-later
// tests/tst_rotorlog_breite.cpp  (Longpath)
//
// Longpath-original. No Thetis port.
// no-port-check: Longpath-original.
//
// Rotor/Log breit gezogen (2026-11-26). Betreiber: „wenn ich es in die
// breite ziehe sollte dort dann der rest, sofern möglich sein, stehen,
// also call usw." Ist das Feld breiter als hoch und breit genug, stehen
// Rufzeichen, Karte, Peilung und Log neben dem Kompass statt zu
// verschwinden. Schmal bleibt alles wie vorher.
//
// Bei jeder Breite: nichts ragt aus dem Feld, keine Beschriftung ist
// abgeschnitten, und der Kompass bleibt gross genug zum Zielen.
//
// Mit LONGPATH_GRAB_DIR legt der Pruefstand Bilder des echten Felds ab
// (mit QT_SCALE_FACTOR=2 in wirklicher Groesse).
// =================================================================
// Modification history (Longpath):
//   2026-11-26 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest>

#include "core/CallsignInfo.h"
#include "core/QrzClient.h"
#include "core/RotorBereich.h"
#include "gui/widgets/RotorDialWidget.h"
#include "gui/widgets/RotorLogbookPanel.h"
#include "gui/widgets/StationPhoto.h"
#include "FensterFrist.h"

#include <QApplication>
#include <QDateTime>
#include <QMouseEvent>
#include <QSignalSpy>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

using namespace Longpath;

namespace {

QLineEdit* feld(QWidget& w, const QString& platzhalter)
{
    for (QLineEdit* e : w.findChildren<QLineEdit*>()) {
        if (e->placeholderText() == platzhalter) { return e; }
    }
    return nullptr;
}

void bild(QWidget& w, const QString& name)
{
    const QString dir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
    if (dir.isEmpty()) { return; }
    QDir().mkpath(dir);
    w.grab().save(dir + QLatin1Char('/') + name);
}

void legeFotoAb(const QString& url)
{
    const QString ziel = StationPhoto::cachePath(url);
    QDir().mkpath(QFileInfo(ziel).absolutePath());
    QFile::remove(ziel);
    const QString vonPlatte = qEnvironmentVariable("LONGPATH_GRAB_FOTO");
    if (!vonPlatte.isEmpty() && QFile::copy(vonPlatte, ziel)) { return; }
    QImage b(480, 360, QImage::Format_RGB32);
    QPainter p(&b);
    QLinearGradient himmel(0, 0, 0, 360);
    himmel.setColorAt(0.0, QColor(0x6f, 0x8f, 0xb5));
    himmel.setColorAt(0.7, QColor(0xc9, 0xd6, 0xe0));
    himmel.setColorAt(1.0, QColor(0x5d, 0x6b, 0x4a));
    p.fillRect(b.rect(), himmel);
    p.fillRect(QRect(0, 280, 480, 80), QColor(0x55, 0x63, 0x42));
    p.setPen(QPen(QColor(0x33, 0x33, 0x38), 3));
    p.drawLine(300, 280, 300, 80);
    p.drawLine(250, 90, 350, 90);
    p.drawLine(260, 105, 340, 105);
    p.fillRect(QRect(110, 210, 120, 70), QColor(0xb8, 0x9a, 0x6a));
    p.end();
    b.save(ziel, "PNG");
}

// Ein paar QSOs ins Log, damit die Liste etwas zeigt.
void legeLogAn()
{
    const QString pfad = RotorLogbookPanel::logbookPath();
    QDir().mkpath(QFileInfo(pfad).absolutePath());
    QFile f(pfad);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write("Longpath test log\n<EOH>\n");
    const char* zeilen[] = {
        "<CALL:6>VE2ABC <QSO_DATE:8>20261126 <TIME_ON:6>181530 <FREQ:9>14.075850 <BAND:3>20m <MODE:3>FT8 <EOR>\n",
        "<CALL:5>K1XYZ <QSO_DATE:8>20261126 <TIME_ON:6>180215 <FREQ:9>14.076100 <BAND:3>20m <MODE:3>FT8 <EOR>\n",
        "<CALL:6>JA1ABC <QSO_DATE:8>20261126 <TIME_ON:6>071000 <FREQ:9>21.074900 <BAND:3>15m <MODE:3>FT8 <EOR>\n",
        "<CALL:6>PY2ABC <QSO_DATE:8>20261125 <TIME_ON:6>203000 <FREQ:9>28.074400 <BAND:3>10m <MODE:3>FT8 <EOR>\n",
    };
    for (const char* z : zeilen) { f.write(z); }
}

CallsignInfo antwort(const QString& bildUrl)
{
    CallsignInfo i;
    i.call         = QStringLiteral("VE2ABC");
    i.firstName    = QStringLiteral("Marc");
    i.lastName     = QStringLiteral("Tremblay");
    i.city         = QStringLiteral("Montreal");
    i.country      = QStringLiteral("Canada");
    i.grid         = QStringLiteral("FN35FL");
    i.licenseClass = QStringLiteral("A");
    i.imageUrl     = bildUrl;
    i.fetchedUtc   = QDateTime::currentSecsSinceEpoch();
    return i;
}

// Was sichtbar ist, liegt ganz im Feld, und keine einzeilige Beschriftung
// ist kuerzer als ihr Text.
QString abgeschnitten(QWidget& p)
{
    const QRect feldRect = p.rect();
    for (QWidget* w : p.findChildren<QWidget*>()) {
        if (!w->isVisibleTo(&p) || w->isWindow()) { continue; }
        // Nur, was im Feld selbst liegt (nicht im Inneren der Tabelle).
        if (w->parentWidget() && w->parentWidget()->inherits("QAbstractScrollArea")) { continue; }
        if (w->parentWidget() && w->parentWidget()->parentWidget()
            && w->parentWidget()->parentWidget()->inherits("QAbstractScrollArea")) { continue; }
        const QRect r(w->mapTo(&p, QPoint(0, 0)), w->size());
        if (!feldRect.contains(r)) {
            return QStringLiteral("%1 \"%2\" ragt hinaus: %3,%4 %5x%6 in %7x%8")
                .arg(QString::fromLatin1(w->metaObject()->className()), w->objectName())
                .arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height())
                .arg(feldRect.width()).arg(feldRect.height());
        }
        if (auto* l = qobject_cast<QLabel*>(w)) {
            if (!l->wordWrap() && !l->text().isEmpty() && l->pixmap().isNull()) {
                const int brauch = l->fontMetrics().horizontalAdvance(l->text());
                if (brauch > l->contentsRect().width() + 1) {
                    return QStringLiteral("Beschriftung \"%1\" abgeschnitten (%2 von %3)")
                        .arg(l->text()).arg(l->contentsRect().width()).arg(brauch);
                }
            }
            if (l->height() < l->minimumSizeHint().height() - 1) {
                return QStringLiteral("Beschriftung \"%1\" zu niedrig").arg(l->text());
            }
        }
        if (auto* e = qobject_cast<QLineEdit*>(w)) {
            const QString t = e->text().isEmpty() ? e->placeholderText() : e->text();
            const int brauch = e->fontMetrics().horizontalAdvance(t);
            if (!t.isEmpty() && brauch + 12 > e->width()) {
                return QStringLiteral("Feld \"%1\" zu schmal (%2 fuer %3)")
                    .arg(t).arg(e->width()).arg(brauch);
            }
        }
        if (auto* b = qobject_cast<QPushButton*>(w)) {
            const int brauch = b->fontMetrics().horizontalAdvance(b->text());
            if (brauch > b->width()) {
                return QStringLiteral("Knopf \"%1\" abgeschnitten").arg(b->text());
            }
        }
    }
    return {};
}

// Der Kompass nimmt einen Klick als Ziel an (irgendwo auf der Rose).
bool kompassNimmtKlick(RotorDialWidget* d)
{
    QSignalSpy spy(d, &RotorDialWidget::targetPicked);
    for (int y = 4; y < d->height(); y += 6) {
        for (int x = 4; x < d->width(); x += 6) {
            const QPointF pt(x, y);
            QMouseEvent druck(QEvent::MouseButtonPress, pt, d->mapToGlobal(pt),
                              Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(d, &druck);
            QMouseEvent los(QEvent::MouseButtonRelease, pt, d->mapToGlobal(pt),
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(d, &los);
            if (!spy.isEmpty()) { return true; }
        }
    }
    return false;
}

bool zeigtText(QWidget& p, const QString& teil)
{
    for (QLabel* l : p.findChildren<QLabel*>()) {
        if (l->isVisibleTo(&p) && l->text().contains(teil)) { return true; }
    }
    return false;
}

} // namespace

class TstRotorlogBreite : public QObject { Q_OBJECT
private:
    void fuellen(RotorLogbookPanel& p)
    {
        feld(p, QStringLiteral("your locator"))->setText(QStringLiteral("JN67VV"));
        const QString url = QStringLiteral("https://example.invalid/pruefstand-ve2abc.png");
        legeFotoAb(url);
        p.merkeQrzAntwortForTest(antwort(url));
        p.folgeFt8Partner(QStringLiteral("VE2ABC"));
        p.lookupTimerForTest()->stop();
    }

private slots:
    void initTestCase()
    {
        RotorBereich frei;
        frei.save();
        legeLogAn();
    }

    // Bei jeder Breite (240 hoch wie im Bild des Betreibers vom
    // 26.11.): nichts ragt hinaus, nichts ist abgeschnitten, der Kompass
    // nimmt Klicks. Breit steht neben ihm, wer die Station ist.
    void jede_breite_data()
    {
        QTest::addColumn<int>("breite");
        QTest::addColumn<int>("form");
        QTest::addColumn<bool>("ruf");      // Rufzeichen und Peilung zu sehen
        QTest::addColumn<bool>("foto");
        QTest::newRow("schmal-330")  << 330  << int(RotorLogbookPanel::Form::Schmal) << false << false;
        QTest::newRow("mittel-520")  << 520  << int(RotorLogbookPanel::Form::Mittel) << true  << false;
        QTest::newRow("breit-740")   << 740  << int(RotorLogbookPanel::Form::Breit)  << true  << true;
        QTest::newRow("breit-1000")  << 1000 << int(RotorLogbookPanel::Form::Breit)  << true  << true;
    }
    void jede_breite()
    {
        QFETCH(int, breite);
        QFETCH(int, form);
        QFETCH(bool, ruf);
        QFETCH(bool, foto);
        QrzClient qrz;
        qrz.setCredentials(QStringLiteral("pruefstand"), QStringLiteral("pruefstand"));
        // Wie im Dock: das Feld als Kind eines Halters. Als eigenes
        // Fenster zoege Qt es auf die Mindesthoehe seines Layouts.
        QWidget halter;
        halter.resize(1000, 240);
        auto* p = new RotorLogbookPanel(nullptr, &qrz, nullptr, &halter);
        p->setGeometry(0, 0, 330, 240);
        halter.show();
        QVERIFY(QTest::qWaitForWindowExposed(&halter, kFensterFrist));
        fuellen(*p);
        p->setGeometry(0, 0, breite, 240);
        QTest::qWait(250);
        QCOMPARE(p->size(), QSize(breite, 240));
        QCOMPARE(int(p->form()), form);

        const QString fehler = abgeschnitten(*p);
        QVERIFY2(fehler.isEmpty(), qPrintable(fehler));
        RotorDialWidget* d = p->dial();
        QVERIFY(d->isVisibleTo(p));
        QVERIFY2(d->width() >= 84 && d->height() >= 84, "Kompass zu klein");
        QVERIFY2(kompassNimmtKlick(d), "der Kompass nimmt keinen Klick an");

        QLineEdit* call = feld(*p, QStringLiteral("callsign"));
        QVERIFY(call);
        QCOMPARE(call->isVisibleTo(p), ruf);
        if (ruf) {
            QVERIFY2(zeigtText(*p, QStringLiteral("Marc Tremblay")), "Name fehlt");
            QVERIFY2(zeigtText(*p, QStringLiteral("Canada")), "Land fehlt");
            QVERIFY2(zeigtText(*p, QStringLiteral("303°")), "Kurzweg fehlt");
            QVERIFY2(zeigtText(*p, QStringLiteral("123°")), "Langweg fehlt");
            QVERIFY2(zeigtText(*p, QStringLiteral("6291 km")), "Entfernung fehlt");
            QLineEdit* loc = feld(*p, QStringLiteral("locator"));
            QVERIFY(loc && loc->isVisibleTo(p) && loc->text() == QStringLiteral("FN35FL"));
            // Rufzeichen und Karte stehen NEBEN dem Kompass, nicht darueber.
            const QRect kompass(d->mapTo(p, QPoint(0, 0)), d->size());
            QVERIFY(call->mapTo(p, QPoint(0, 0)).x() >= kompass.right());
            QPushButton* log = nullptr;
            for (QPushButton* b : p->findChildren<QPushButton*>()) {
                if (b->text() == QStringLiteral("Log QSO")) { log = b; }
            }
            QVERIFY2(log && log->isVisibleTo(p), "Log QSO fehlt");
        }
        StationPhoto* bildchen = p->findChild<StationPhoto*>();
        QVERIFY(bildchen && bildchen->hasPhoto());
        QCOMPARE(bildchen->isVisibleTo(p), foto);
        bild(*p, QStringLiteral("A-%1.png").arg(breite));

        // Und zurueck: schmal wieder wie vorher.
        p->setGeometry(0, 0, 330, 240);
        QTest::qWait(100);
        QCOMPARE(p->form(), RotorLogbookPanel::Form::Schmal);
        QVERIFY(!call->isVisibleTo(p));
        QVERIFY2(abgeschnitten(*p).isEmpty(), qPrintable(abgeschnitten(*p)));
    }

    // Als schwebendes Fenster (2026-11-26, 20:30, am Geraet): „hat sich
    // nun so groß gemacht, dass man es nicht mehr kleiner machen kann".
    // Ein Fenster mit Layout wird auf die Mindestgroesse seines Inhalts
    // gezogen -- sie darf nie an den gerade sichtbaren Zeilen haengen.
    void schwebend_wieder_klein()
    {
        QrzClient qrz;
        qrz.setCredentials(QStringLiteral("pruefstand"), QStringLiteral("pruefstand"));
        QWidget fenster;                       // wie ToolWindow: Fenster + Layout
        auto* lay = new QVBoxLayout(&fenster);
        lay->setContentsMargins(0, 0, 0, 0);
        // Wie im Programm: das Feld entsteht ohne Groesse (Qt: 640 x 480,
        // also „breit") und kommt erst dann ins schmale Fenster.
        auto* p = new RotorLogbookPanel(nullptr, &qrz, nullptr);
        p->resize(640, 480);
        lay->addWidget(p);
        fenster.resize(330, 240);
        fenster.show();
        QVERIFY(QTest::qWaitForWindowExposed(&fenster, kFensterFrist));
        fuellen(*p);
        QTest::qWait(250);
        QCOMPARE(fenster.size(), QSize(330, 240));

        // Breit, schmal und hoch, wieder klein -- jedes Mal muss das
        // Fenster die Groesse annehmen, die es bekommt.
        const QVector<QSize> zuege = {{740, 240}, {330, 240}, {330, 733},
                                      {330, 240}, {1000, 300}, {330, 240}};
        for (const QSize z : zuege) {
            fenster.resize(z);
            QTest::qWait(150);
            QVERIFY2(fenster.size() == z, qPrintable(QStringLiteral("gezogen auf %1x%2, steht bei %3x%4")
                .arg(z.width()).arg(z.height()).arg(fenster.width()).arg(fenster.height())));
            // Die Mindestgroesse: nie mehr als Rand + Kompass hoch und
            // das schmale Feld breit -- egal, was gerade zu sehen ist.
            const QSize min = fenster.minimumSizeHint();
            QVERIFY2(min.height() <= 8 + 8 + p->dial()->parentWidget()->minimumSizeHint().height()
                         && min.width() <= 20 + 300,
                     qPrintable(QStringLiteral("Mindestgroesse %1x%2 nach %3x%4")
                .arg(min.width()).arg(min.height()).arg(z.width()).arg(z.height())));
        }
        QVERIFY2(abgeschnitten(*p).isEmpty(), qPrintable(abgeschnitten(*p)));
    }

    // Ein hohes Dock bleibt die eine Spalte von heute, auch wenn es
    // breiter ist als das uebliche: alles da, Foto gross ueber dem Namen.
    void hohes_dock_wie_bisher()
    {
        QrzClient qrz;
        qrz.setCredentials(QStringLiteral("pruefstand"), QStringLiteral("pruefstand"));
        QWidget halter;
        halter.resize(600, 900);
        auto* p = new RotorLogbookPanel(nullptr, &qrz, nullptr, &halter);
        p->setGeometry(0, 0, 600, 900);
        halter.show();
        QVERIFY(QTest::qWaitForWindowExposed(&halter, kFensterFrist));
        fuellen(*p);
        QTest::qWait(250);
        QCOMPARE(p->form(), RotorLogbookPanel::Form::Schmal);
        auto* foto = p->findChild<StationPhoto*>();
        QTRY_VERIFY(foto->isVisibleTo(p));
        QCOMPARE(foto->size(), QSize(300, 225));
        QLineEdit* call = feld(*p, QStringLiteral("callsign"));
        QVERIFY(call->mapTo(p, QPoint(0, 0)).y() < p->dial()->mapTo(p, QPoint(0, 0)).y());
        QVERIFY2(abgeschnitten(*p).isEmpty(), qPrintable(abgeschnitten(*p)));
    }
};

QTEST_MAIN(TstRotorlogBreite)
#include "tst_rotorlog_breite.moc"
