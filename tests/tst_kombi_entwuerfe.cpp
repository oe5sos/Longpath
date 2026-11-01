// no-port-check: Longpath-eigenes Zeichenwerkzeug, kein Prüfstand.
// =================================================================
// tests/tst_kombi_entwuerfe.cpp  (Longpath)
// =================================================================
//
// Kein Prüfstand — ein Zeichenwerkzeug. Vier Entwürfe für den
// Umschalter des Kombi-Feldes, je ein Blatt, und auf jedem Blatt
// BEIDE Betriebsfälle (RX vorn / TX vorn).
//
// Betreiber, 2026-11-01: „gerne vorab vorschläge...." — also vor dem
// Umbau des sichtbaren Teils erst die Blätter.
//
//   LONGPATH_GRAB_DIR=/tmp/blaetter LONGPATH_ENTWURF=1 \
//     QT_QPA_PLATFORM=offscreen ./build/tests/tst_kombi_entwuerfe
// =================================================================
// Modification history (Longpath):
//   2026-11-01 — Neu, zum Umschalter des Kombi-Feldes.
//                Longpath, KI-gestützt via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QStyleFactory>

#include "gui/StyleConstants.h"
#include "gui/applets/GridCellWidget.h"
#include "gui/applets/KombiApplet.h"
#include "gui/applets/RxApplet.h"
#include "gui/applets/TxApplet.h"
#include "gui/styles/AppTheme.h"
#include "models/RadioModel.h"

using namespace Longpath;

namespace {

constexpr int kBreite  = 360;   // wie ein schmales Panel beim Betreiber
constexpr int kHoehe   = 150;
constexpr int kRand    = 16;
constexpr int kKopfH   = 18;

QColor c(const char* hex) { return QColor(QLatin1String(hex)); }

// Der Panelrahmen, wie ihn der Betreiber kennt: dunkle Fläche,
// schmale Kopfzeile, ein Punkt Rahmen.
QRect maleRahmen(QPainter& p, const QRect& r, const QString& titel)
{
    p.fillRect(r, c(Style::kPanelBg));
    p.setPen(QPen(c(Style::kBorder), 1));
    p.drawRect(r.adjusted(0, 0, -1, -1));

    const QRect kopf(r.left(), r.top(), r.width(), kKopfH);
    p.fillRect(kopf, c(Style::kButtonBg));
    p.setPen(QPen(c(Style::kBorder), 1));
    p.drawLine(kopf.bottomLeft(), kopf.bottomRight());

    QFont f = p.font();
    f.setPointSizeF(8.0);
    f.setBold(true);
    p.setFont(f);
    p.setPen(c(Style::kTextTertiary));
    p.drawText(kopf.adjusted(8, 0, -40, 0), Qt::AlignVCenter | Qt::AlignLeft,
               titel);
    // Zahnrad und × rechts oben — Martins feste Regel seit 2026-09-20.
    p.setPen(c(Style::kTextTertiary));
    p.drawText(kopf.adjusted(0, 0, -22, 0), Qt::AlignVCenter | Qt::AlignRight,
               QStringLiteral("⚙"));
    p.drawText(kopf.adjusted(0, 0, -8, 0), Qt::AlignVCenter | Qt::AlignRight,
               QStringLiteral("✕"));
    return QRect(r.left(), kopf.bottom() + 1, r.width(),
                 r.bottom() - kopf.bottom() - 1);
}

// Angedeuteter Inhalt: zwei Regler und eine Knopfreihe. Es geht um
// den Umschalter, nicht um die Seite darunter — aber ganz ohne
// Inhalt lässt sich die Wirkung nicht beurteilen.
void maleInhalt(QPainter& p, const QRect& r, bool tx)
{
    QFont f = p.font();
    f.setPointSizeF(7.5);
    f.setBold(false);
    p.setFont(f);

    const QStringList zeilen = tx
        ? QStringList{QStringLiteral("POWER"), QStringLiteral("TUNE"),
                      QStringLiteral("MIC")}
        : QStringList{QStringLiteral("AF"), QStringLiteral("SQL"),
                      QStringLiteral("AGC-T")};
    int y = r.top() + 12;
    for (const QString& z : zeilen) {
        p.setPen(c(Style::kTextTertiary));
        p.drawText(QRect(r.left() + 10, y - 6, 58, 12),
                   Qt::AlignVCenter | Qt::AlignLeft, z);
        const QRect bahn(r.left() + 72, y - 2, r.width() - 90, 4);
        p.fillRect(bahn, c(Style::kButtonBg));
        p.setPen(Qt::NoPen);
        p.setBrush(c(Style::kTextTertiary));
        const int x = bahn.left() + bahn.width() * (tx ? 0.25 : 0.55);
        p.drawEllipse(QPoint(x, bahn.center().y()), 4, 4);
        p.setBrush(Qt::NoBrush);
        y += 22;
    }
}

void maleKnopf(QPainter& p, const QRect& r, const QString& text, bool aktiv)
{
    QPainterPath pfad;
    pfad.addRoundedRect(r, 5, 5);
    p.fillPath(pfad, c(aktiv ? Style::kAccent : Style::kButtonBg));
    p.setPen(QPen(c(aktiv ? Style::kAccent : Style::kBorder), 1));
    p.drawPath(pfad);
    QFont f = p.font();
    f.setPointSizeF(8.0);
    f.setBold(true);
    p.setFont(f);
    p.setPen(c(aktiv ? Style::kTextPrimary : Style::kTextTertiary));
    p.drawText(r, Qt::AlignCenter, text);
}

}  // namespace

class TestKombiEntwuerfe : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        applyDarkPalette(*qApp);
        applyAppBaselineQss(*qApp);
    }

    void blaetter()
    {
        const QByteArray dirRoh = qgetenv("LONGPATH_GRAB_DIR");
        if (dirRoh.isEmpty() || qgetenv("LONGPATH_ENTWURF") != "1") {
            QSKIP("Zeichenwerkzeug: LONGPATH_GRAB_DIR + LONGPATH_ENTWURF=1");
        }
        const QString dir = QString::fromLocal8Bit(dirRoh);
        QDir().mkpath(dir);

        malen(dir, 1, QStringLiteral("Knopfreihe oben links"));
        malen(dir, 2, QStringLiteral("Segmentleiste ueber die Breite"));
        malen(dir, 3, QStringLiteral("Namen im Panelkopf"));
        malen(dir, 4, QStringLiteral("Unterstrich-Reiter"));
    }

    // Zweite Runde: der Umschalter IN der Kopfleiste.
    //
    // Betreiber, 2026-11-01: "das TX panel ist oben so gross, das bei
    // meinem mac das nicht otimal ist. andere vorschlaege bitte" und
    // "ev. die Leiste, wo jetzt RX1 Panel als Menue verwenden."
    //
    // Also: der eigene Reiterstreifen faellt weg, der Umschalter zieht
    // in die Kopfleiste, die ohnehin schon da ist.
    void kopfleiste()
    {
        const QByteArray dirRoh = qgetenv("LONGPATH_GRAB_DIR");
        if (dirRoh.isEmpty() || qgetenv("LONGPATH_ENTWURF") != "1") {
            QSKIP("Zeichenwerkzeug: LONGPATH_GRAB_DIR + LONGPATH_ENTWURF=1");
        }
        const QString dir = QString::fromLocal8Bit(dirRoh);
        malenKopf(dir, 1, QStringLiteral("Reiter statt Titel"));
        malenKopf(dir, 2, QStringLiteral("Titel + Reiter"));
        malenKopf(dir, 3, QStringLiteral("Ein Chip, schaltet weiter"));
        malenKopf(dir, 4, QStringLiteral("Der Titel ist die Seite"));
    }

    // Dritte Runde: die FORM des Umschalters in der Kopfleiste.
    //
    // Betreiber, 2026-11-01, zum gebauten Chip: "aber die pillen
    // gefallen mir nicht". Also dieselbe Stelle, vier Formen.
    void chipformen()
    {
        const QByteArray dirRoh = qgetenv("LONGPATH_GRAB_DIR");
        if (dirRoh.isEmpty() || qgetenv("LONGPATH_ENTWURF") != "1") {
            QSKIP("Zeichenwerkzeug: LONGPATH_GRAB_DIR + LONGPATH_ENTWURF=1");
        }
        const QString dir = QString::fromLocal8Bit(dirRoh);
        malenForm(dir, 1, QStringLiteral("Nur Text, kein Rahmen"));
        malenForm(dir, 2, QStringLiteral("Text mit Strich darunter"));
        malenForm(dir, 3, QStringLiteral("Eckig statt Pille"));
        malenForm(dir, 4, QStringLiteral("Zwei Pfeile, Name dazwischen"));
    }

    // ── Nach der Wahl: das ECHTE Bauteil ────────────────────────────
    //
    // Freihand gemalte Entwuerfe sehen im eingebauten Zustand anders
    // aus -- darum nach der Entscheidung dasselbe noch einmal mit dem
    // wirklichen KombiApplet und den wirklichen RX-/TX-Panels.
    void dasEchteFeld()
    {
        const QByteArray dirRoh = qgetenv("LONGPATH_GRAB_DIR");
        if (dirRoh.isEmpty() || qgetenv("LONGPATH_ENTWURF") != "1") {
            QSKIP("Zeichenwerkzeug: LONGPATH_GRAB_DIR + LONGPATH_ENTWURF=1");
        }
        const QString dir = QString::fromLocal8Bit(dirRoh);

        // Das Feld IN SEINEM GEHAEUSE -- der Umschalter sitzt seit
        // Entwurf 3 in der Kopfleiste der Zelle, also zeigt ein Bild
        // des Applets allein ihn gar nicht.
        RadioModel rm;
        GridCellWidget zelle(QStringLiteral("probe"));
        KombiApplet feld(&rm);
        auto* rx = new RxApplet(nullptr, &rm, nullptr);
        auto* tx = new TxApplet(&rm, nullptr);
        feld.nimmAuf(rx);
        feld.nimmAuf(tx);
        // Eine DRITTE Seite dazu und die Zelle schmal: genau der
        // Fall, nach dem der Betreiber gefragt hat („wäre platz, eine
        // dritte" / „oder man macht es zweizeilig"). Gemessen statt
        // geraten.
        auto* rotor = new QWidget;
        rotor->setWindowTitle(QStringLiteral("Rotor / Log"));
        feld.nimmAuf(rotor);
        zelle.addWidget(&feld);
        zelle.resize(300, 300);
        zelle.show();

        for (int i = 0; i < 2; ++i) {
            feld.zeigeSeite(i);
            QTest::qWait(300);
            const QImage bild = zelle.grab().toImage();
            const QString pfad = QStringLiteral("%1/kombi-echt-%2.png")
                                     .arg(dir)
                                     .arg(i == 0 ? QStringLiteral("rx")
                                                 : QStringLiteral("tx"));
            QVERIFY2(bild.save(pfad), qPrintable(pfad));
            qInfo() << "Blatt:" << pfad << bild.size();
        }
    }

private:
    // Nur die Kopfleiste, gross genug zum Beurteilen: die Form des
    // Umschalters ist das Thema, nicht der Inhalt darunter.
    void malenForm(const QString& dir, int nummer, const QString& name)
    {
        const int blattB = kBreite + 2 * kRand;
        const int blattH = 2 * 54 + 3 * kRand + 34;
        QImage bild(blattB * 2, blattH * 2, QImage::Format_ARGB32_Premultiplied);
        bild.setDevicePixelRatio(2.0);
        bild.setDotsPerMeterX(int(144 / 0.0254));
        bild.setDotsPerMeterY(int(144 / 0.0254));
        bild.fill(c("#141418"));

        QPainter p(&bild);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        QFont kopf = p.font();
        kopf.setPointSizeF(9.0);
        kopf.setBold(true);
        p.setFont(kopf);
        p.setPen(c(Style::kTextPrimary));
        p.drawText(QRect(kRand, 6, blattB - 2 * kRand, 18),
                   Qt::AlignVCenter | Qt::AlignLeft,
                   QStringLiteral("Form %1 — %2").arg(nummer).arg(name));

        for (int i = 0; i < 2; ++i) {
            const QRect feld(kRand, 30 + kRand + i * (54 + kRand), kBreite, 54);
            maleForm(p, feld, nummer, i == 1);
        }
        p.end();
        const QString pfad = QStringLiteral("%1/kombi-form-%2.png")
                                 .arg(dir).arg(nummer);
        QVERIFY2(bild.save(pfad), qPrintable(pfad));
        qInfo() << "Blatt:" << pfad << bild.size();
    }

    void maleForm(QPainter& p, const QRect& feld, int nummer, bool tx)
    {
        p.fillRect(feld, c(Style::kPanelBg));
        p.setPen(QPen(c(Style::kBorder), 1));
        p.drawRect(feld.adjusted(0, 0, -1, -1));
        const QRect kopfR(feld.left(), feld.top(), feld.width(), kKopfH + 4);
        p.fillRect(kopfR, c(Style::kButtonBg));
        p.drawLine(kopfR.bottomLeft(), kopfR.bottomRight());

        QFont f = p.font();
        f.setPointSizeF(8.0);
        f.setBold(true);
        p.setFont(f);
        p.setPen(c(Style::kTextTertiary));
        p.drawText(kopfR.adjusted(8, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft,
                   QStringLiteral("COMBINED"));

        const QString an = tx ? QStringLiteral("TX") : QStringLiteral("RX");
        const QRect platz(kopfR.left() + 116, kopfR.top() + 3, 62,
                          kopfR.height() - 6);

        if (nummer == 1) {
            p.setPen(c(Style::kTextPrimary));
            p.drawText(platz, Qt::AlignVCenter | Qt::AlignLeft,
                       QStringLiteral("%1 ›").arg(an));
        } else if (nummer == 2) {
            p.setPen(c(Style::kTextPrimary));
            p.drawText(platz, Qt::AlignVCenter | Qt::AlignLeft,
                       QStringLiteral("%1 ›").arg(an));
            const QRect strich(platz.left(), platz.bottom() - 1, 30, 1);
            p.fillRect(strich, c(Style::kTextTertiary));
        } else if (nummer == 3) {
            p.fillRect(platz, c(Style::kPanelBg));
            p.setPen(QPen(c(Style::kBorder), 1));
            p.drawRect(platz);
            p.setPen(c(Style::kTextPrimary));
            p.drawText(platz, Qt::AlignCenter,
                       QStringLiteral("%1 ›").arg(an));
        } else {
            p.setPen(c(Style::kTextTertiary));
            p.drawText(QRect(platz.left(), platz.top(), 12, platz.height()),
                       Qt::AlignCenter, QStringLiteral("‹"));
            p.drawText(QRect(platz.right() - 12, platz.top(), 12,
                             platz.height()),
                       Qt::AlignCenter, QStringLiteral("›"));
            p.setPen(c(Style::kTextPrimary));
            p.drawText(platz, Qt::AlignCenter, an);
        }

        p.setPen(c(Style::kTextTertiary));
        p.drawText(kopfR.adjusted(0, 0, -22, 0),
                   Qt::AlignVCenter | Qt::AlignRight, QStringLiteral("⚙"));
        p.drawText(kopfR.adjusted(0, 0, -8, 0),
                   Qt::AlignVCenter | Qt::AlignRight, QStringLiteral("✕"));

        QFont k = p.font();
        k.setPointSizeF(7.5);
        k.setBold(false);
        p.setFont(k);
        p.setPen(c(Style::kTextInactive));
        p.drawText(QRect(feld.left() + 10, kopfR.bottom() + 6,
                         feld.width() - 20, 18),
                   Qt::AlignVCenter | Qt::AlignLeft,
                   tx ? QStringLiteral("POWER   TUNE   MIC")
                      : QStringLiteral("AF   SQL   AGC-T"));
    }

    void malenKopf(const QString& dir, int nummer, const QString& name)
    {
        const int blattB = kBreite + 2 * kRand;
        const int blattH = 2 * kHoehe + 3 * kRand + 34;
        QImage bild(blattB * 2, blattH * 2, QImage::Format_ARGB32_Premultiplied);
        bild.setDevicePixelRatio(2.0);
        bild.setDotsPerMeterX(int(144 / 0.0254));
        bild.setDotsPerMeterY(int(144 / 0.0254));
        bild.fill(c("#141418"));

        QPainter p(&bild);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        QFont kopf = p.font();
        kopf.setPointSizeF(9.0);
        kopf.setBold(true);
        p.setFont(kopf);
        p.setPen(c(Style::kTextPrimary));
        p.drawText(QRect(kRand, 6, blattB - 2 * kRand, 18),
                   Qt::AlignVCenter | Qt::AlignLeft,
                   QStringLiteral("Kopfleiste %1 — %2").arg(nummer).arg(name));
        for (int i = 0; i < 2; ++i) {
            const QRect feld(kRand, 30 + kRand + i * (kHoehe + kRand),
                             kBreite, kHoehe);
            maleKopfEntwurf(p, feld, nummer, i == 1);
        }
        p.end();
        const QString pfad = QStringLiteral("%1/kombi-kopf-%2.png")
                                 .arg(dir).arg(nummer);
        QVERIFY2(bild.save(pfad), qPrintable(pfad));
        qInfo() << "Blatt:" << pfad << bild.size();
    }

    // Alle vier teilen sich denselben Rahmen: EIN Streifen oben, kein
    // zweiter darunter. Genau das ist der Gewinn.
    void maleKopfEntwurf(QPainter& p, const QRect& feld, int nummer, bool tx)
    {
        p.fillRect(feld, c(Style::kPanelBg));
        p.setPen(QPen(c(Style::kBorder), 1));
        p.drawRect(feld.adjusted(0, 0, -1, -1));
        const QRect kopfR(feld.left(), feld.top(), feld.width(), kKopfH);
        p.fillRect(kopfR, c(Style::kButtonBg));
        p.drawLine(kopfR.bottomLeft(), kopfR.bottomRight());

        QFont f = p.font();
        f.setPointSizeF(8.0);
        f.setBold(true);
        p.setFont(f);
        const QString an  = tx ? QStringLiteral("TX") : QStringLiteral("RX");
        const QString aus = tx ? QStringLiteral("RX") : QStringLiteral("TX");

        if (nummer == 1) {
            int x = kopfR.left() + 8;
            for (int i = 0; i < 2; ++i) {
                const bool dieser = (i == 1) == tx;
                const QRect tr(x, kopfR.top(), 28, kopfR.height());
                if (dieser) { p.fillRect(tr, c(Style::kPanelBg)); }
                p.setPen(c(dieser ? Style::kTextPrimary : Style::kTextInactive));
                p.drawText(tr, Qt::AlignCenter,
                           i == 0 ? QStringLiteral("RX") : QStringLiteral("TX"));
                x += 30;
            }
        } else if (nummer == 2) {
            p.setPen(c(Style::kTextTertiary));
            p.drawText(kopfR.adjusted(8, 0, 0, 0),
                       Qt::AlignVCenter | Qt::AlignLeft,
                       QStringLiteral("COMBINED"));
            int x = kopfR.left() + 116;
            for (int i = 0; i < 2; ++i) {
                const bool dieser = (i == 1) == tx;
                const QRect tr(x, kopfR.top() + 2, 28, kopfR.height() - 4);
                if (dieser) {
                    p.fillRect(tr, c(Style::kPanelBg));
                    p.setPen(QPen(c(Style::kBorder), 1));
                    p.drawRect(tr);
                }
                p.setPen(c(dieser ? Style::kTextPrimary : Style::kTextInactive));
                p.drawText(tr, Qt::AlignCenter,
                           i == 0 ? QStringLiteral("RX") : QStringLiteral("TX"));
                x += 30;
            }
        } else if (nummer == 3) {
            p.setPen(c(Style::kTextTertiary));
            p.drawText(kopfR.adjusted(8, 0, 0, 0),
                       Qt::AlignVCenter | Qt::AlignLeft,
                       QStringLiteral("COMBINED"));
            const QRect chip(kopfR.left() + 116, kopfR.top() + 2, 54,
                             kopfR.height() - 4);
            p.fillRect(chip, c(Style::kPanelBg));
            p.setPen(QPen(c(Style::kBorder), 1));
            p.drawRect(chip);
            p.setPen(c(Style::kTextPrimary));
            p.drawText(chip, Qt::AlignCenter,
                       QStringLiteral("%1 ›").arg(an));
        } else {
            p.setPen(c(Style::kTextPrimary));
            p.drawText(kopfR.adjusted(8, 0, 0, 0),
                       Qt::AlignVCenter | Qt::AlignLeft,
                       QStringLiteral("%1  ▾").arg(an));
            p.setPen(c(Style::kTextInactive));
            p.drawText(kopfR.adjusted(0, 0, -44, 0),
                       Qt::AlignVCenter | Qt::AlignRight, aus);
        }

        p.setPen(c(Style::kTextTertiary));
        p.drawText(kopfR.adjusted(0, 0, -22, 0),
                   Qt::AlignVCenter | Qt::AlignRight, QStringLiteral("⚙"));
        p.drawText(kopfR.adjusted(0, 0, -8, 0),
                   Qt::AlignVCenter | Qt::AlignRight, QStringLiteral("✕"));
        maleInhalt(p, QRect(feld.left(), kopfR.bottom() + 1, feld.width(),
                            feld.bottom() - kopfR.bottom() - 1), tx);
    }

    // Ein Blatt: derselbe Entwurf zweimal, einmal mit RX vorn und
    // einmal mit TX vorn. Ein Umschalter, den man nur in einem
    // Zustand sieht, sagt nichts.
    void malen(const QString& dir, int nummer, const QString& name)
    {
        const int blattB = kBreite + 2 * kRand;
        const int blattH = 2 * kHoehe + 3 * kRand + 34;

        QImage bild(blattB * 2, blattH * 2, QImage::Format_ARGB32_Premultiplied);
        bild.setDevicePixelRatio(2.0);
        // 144 dpi: in der Vorschau erscheint das Blatt in WIRKLICHER
        // Groesse -- er entscheidet am Notebook, nicht an der
        // Vergroesserung.
        bild.setDotsPerMeterX(int(144 / 0.0254));
        bild.setDotsPerMeterY(int(144 / 0.0254));
        bild.fill(c("#141418"));

        QPainter p(&bild);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);

        QFont kopf = p.font();
        kopf.setPointSizeF(9.0);
        kopf.setBold(true);
        p.setFont(kopf);
        p.setPen(c(Style::kTextPrimary));
        p.drawText(QRect(kRand, 6, blattB - 2 * kRand, 18),
                   Qt::AlignVCenter | Qt::AlignLeft,
                   QStringLiteral("%1 — %2").arg(nummer).arg(name));

        for (int i = 0; i < 2; ++i) {
            const bool tx = (i == 1);
            const QRect feld(kRand, 30 + kRand + i * (kHoehe + kRand),
                             kBreite, kHoehe);
            maleEntwurf(p, feld, nummer, tx);
        }
        p.end();

        const QString pfad = QStringLiteral("%1/kombi-entwurf-%2.png")
                                 .arg(dir).arg(nummer);
        QVERIFY2(bild.save(pfad), qPrintable(pfad));
        qInfo() << "Blatt:" << pfad << bild.size();
    }

    void maleEntwurf(QPainter& p, const QRect& feld, int nummer, bool tx)
    {
        switch (nummer) {
        case 1: {
            // Wie gebaut: zwei Knoepfe im Inhalt, oben links.
            const QRect inhalt = maleRahmen(p, feld, QStringLiteral("RX / TX"));
            const int h = 18;
            maleKnopf(p, QRect(inhalt.left() + 8, inhalt.top() + 6, 42, h),
                      QStringLiteral("RX"), !tx);
            maleKnopf(p, QRect(inhalt.left() + 54, inhalt.top() + 6, 42, h),
                      QStringLiteral("TX"), tx);
            maleInhalt(p, inhalt.adjusted(0, 28, 0, 0), tx);
            break;
        }
        case 2: {
            // Eine Leiste ueber die volle Breite, zwei gleiche Haelften.
            const QRect inhalt = maleRahmen(p, feld, QStringLiteral("RX / TX"));
            const int h = 20;
            const int halb = (inhalt.width() - 16) / 2;
            maleKnopf(p, QRect(inhalt.left() + 8, inhalt.top() + 6, halb, h),
                      QStringLiteral("RX"), !tx);
            maleKnopf(p, QRect(inhalt.left() + 8 + halb, inhalt.top() + 6,
                               halb, h), QStringLiteral("TX"), tx);
            maleInhalt(p, inhalt.adjusted(0, 30, 0, 0), tx);
            break;
        }
        case 3: {
            // Die Namen stehen IM Kopf, wo sonst der Titel steht --
            // kein zusaetzlicher Streifen, der Platz kostet.
            p.fillRect(feld, c(Style::kPanelBg));
            p.setPen(QPen(c(Style::kBorder), 1));
            p.drawRect(feld.adjusted(0, 0, -1, -1));
            const QRect kopfR(feld.left(), feld.top(), feld.width(), kKopfH);
            p.fillRect(kopfR, c(Style::kButtonBg));
            p.drawLine(kopfR.bottomLeft(), kopfR.bottomRight());

            QFont f = p.font();
            f.setPointSizeF(8.0);
            f.setBold(true);
            p.setFont(f);
            int x = kopfR.left() + 8;
            for (int i = 0; i < 2; ++i) {
                const bool dieser = (i == 1) == tx;
                const QString t = (i == 0) ? QStringLiteral("RX")
                                           : QStringLiteral("TX");
                const QRect tr(x, kopfR.top(), 26, kopfR.height());
                p.setPen(c(dieser ? Style::kTextPrimary : Style::kTextInactive));
                p.drawText(tr, Qt::AlignCenter, t);
                if (dieser) {
                    p.fillRect(QRect(tr.left() + 2, tr.bottom() - 1,
                                     tr.width() - 4, 2), c(Style::kAccent));
                }
                x += 30;
            }
            p.setPen(c(Style::kTextTertiary));
            p.drawText(kopfR.adjusted(0, 0, -22, 0),
                       Qt::AlignVCenter | Qt::AlignRight,
                       QStringLiteral("⚙"));
            p.drawText(kopfR.adjusted(0, 0, -8, 0),
                       Qt::AlignVCenter | Qt::AlignRight,
                       QStringLiteral("✕"));
            maleInhalt(p, QRect(feld.left(), kopfR.bottom() + 1, feld.width(),
                                feld.bottom() - kopfR.bottom() - 1), tx);
            break;
        }
        default: {
            // Nur Text, der aktive durch eine Linie darunter. Der
            // ruhigste Weg -- Farbe bedeckt hoechstens zwei Prozent
            // (HAUSSTIL).
            const QRect inhalt = maleRahmen(p, feld, QStringLiteral("RX / TX"));
            QFont f = p.font();
            f.setPointSizeF(8.5);
            f.setBold(true);
            p.setFont(f);
            int x = inhalt.left() + 10;
            for (int i = 0; i < 2; ++i) {
                const bool dieser = (i == 1) == tx;
                const QString t = (i == 0) ? QStringLiteral("RX")
                                           : QStringLiteral("TX");
                const QRect tr(x, inhalt.top() + 6, 30, 18);
                p.setPen(c(dieser ? Style::kTextPrimary : Style::kTextInactive));
                p.drawText(tr, Qt::AlignCenter, t);
                if (dieser) {
                    p.fillRect(QRect(tr.left(), tr.bottom() + 1, tr.width(), 2),
                               c(Style::kAccent));
                }
                x += 36;
            }
            p.setPen(QPen(c(Style::kBorder), 1));
            p.drawLine(inhalt.left() + 8, inhalt.top() + 27,
                       inhalt.right() - 8, inhalt.top() + 27);
            maleInhalt(p, inhalt.adjusted(0, 28, 0, 0), tx);
            break;
        }
        }
    }
};

QTEST_MAIN(TestKombiEntwuerfe)
#include "tst_kombi_entwuerfe.moc"
