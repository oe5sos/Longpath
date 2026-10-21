// no-port-check: Longpath-eigenes Entwurfswerkzeug, keine portierte Logik.
// =================================================================
// tests/tst_ps3_entwurfsblaetter.cpp  (Longpath)
// =================================================================
//
// Entwurfsblätter zu zwei Gestaltungsfragen, die PureSignal 3 und der
// neue Rotortreiber offengelassen haben. Kein Prüfstand — ein
// Zeichenwerkzeug: es rendert das ECHTE PsForm in drei Fassungen und
// den Verbinden-Kasten des Rotorfensters in drei Fassungen, je ein PNG.
//
// Ohne LONGPATH_GRAB_DIR überspringt es sich (CI), genau wie
// tst_rendering_werkbank:
//
//   LONGPATH_GRAB_DIR=/pfad ./build/tests/tst_ps3_entwurfsblaetter
//
// Die Blätter kommen mit 144 dpi heraus, damit die Vorschau sie in
// wirklicher Größe zeigt (Regel des Betreibers: er entscheidet am
// Notebook nach Lesbarkeit, nicht an der Vergrößerung).
// =================================================================
// Modification history (Longpath):
//   2026-10-21 — Angelegt. Longpath, KI-gestützt via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QGroupBox>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QFormLayout>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QDialog>

#include "gui/PsForm.h"
#include "gui/styles/Theme.h"
#include "gui/styles/AppTheme.h"
#include <QStyleFactory>
#include "models/RadioModel.h"

using namespace Longpath;

namespace {

// Hausstil: Grau auf Fast-Schwarz, Farbe sparsam. Werte aus
// src/gui/StyleConstants.h gelesen, hier nur fuer das Blattumfeld.
const QColor kBlatt   (18, 19, 21);
const QColor kText    (196, 199, 203);
const QColor kLeise   (122, 126, 132);
const QColor kBlau    ( 90, 150, 220);

void blattSpeichern(const QString& dir, const QString& name,
                    const QImage& inhalt, const QString& titel,
                    const QString& fussnote)
{
    const int rand = 28;
    const int kopf = 54;
    const int fuss = 96;
    QImage blatt(inhalt.width() + 2 * rand,
                 inhalt.height() + kopf + fuss, QImage::Format_RGB32);
    blatt.fill(kBlatt);
    QPainter p(&blatt);
    p.setRenderHint(QPainter::Antialiasing);

    QFont f = p.font();
    f.setPointSizeF(13.0);
    f.setBold(true);
    p.setFont(f);
    p.setPen(kText);
    p.drawText(QRect(rand, 14, blatt.width() - 2 * rand, 24),
               Qt::AlignLeft | Qt::AlignVCenter, titel);

    p.drawImage(rand, kopf, inhalt);

    f.setBold(false);
    f.setPointSizeF(11.0);
    p.setFont(f);
    p.setPen(kLeise);
    p.drawText(QRect(rand, kopf + inhalt.height() + 10,
                     blatt.width() - 2 * rand, fuss - 12),
               Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, fussnote);
    p.end();

    // 144 dpi => die Vorschau zeigt wirkliche Groesse.
    blatt.setDotsPerMeterX(static_cast<int>(144.0 / 0.0254));
    blatt.setDotsPerMeterY(static_cast<int>(144.0 / 0.0254));
    const QString pfad = QDir(dir).filePath(name);
    QVERIFY2(blatt.save(pfad), qPrintable(pfad));
    qInfo() << "Blatt:" << pfad << blatt.size();
}

// Die fuenf Bedienelemente, die PureSignal 3 nicht mehr kennt.
QList<QWidget*> toteRegler(PsForm& form)
{
    QList<QWidget*> raus;
    for (const char* name : {"chkPSPin", "chkPSMap", "chkPSStbl",
                             "chkPSRelaxPtol"}) {
        if (auto* w = form.findChild<QWidget*>(QString::fromLatin1(name))) {
            raus << w;
        }
    }
    // TINT: Beschriftung und Auswahl gehoeren zusammen.
    for (auto* w : form.findChildren<QWidget*>()) {
        if (auto* l = qobject_cast<QLabel*>(w)) {
            if (l->text().contains(QStringLiteral("TINT"))) { raus << l; }
        }
        if (auto* c = qobject_cast<QComboBox*>(w)) {
            if (c->objectName().contains(QStringLiteral("Tint"),
                                         Qt::CaseInsensitive)) { raus << c; }
        }
    }
    return raus;
}

} // namespace

class TestPs3Entwurfsblaetter : public QObject {
    Q_OBJECT

private slots:
    void blaetter()
    {
        const QString dir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (dir.isEmpty()) { QSKIP("LONGPATH_GRAB_DIR nicht gesetzt"); }
        // LONGPATH_GRAB_DIR allein genuegt NICHT als Schalter: die CI
        // setzt es global fuer ihre Haenger-Diagnose (ci.yml:877). Damit
        // lief dieses Zeichenwerkzeug dort mit und warf den Linux-Lauf
        // um -- ein Werkzeug, das Blaetter malt, hat in der CI nichts
        // verloren. Zweiter, ausdruecklicher Schalter.
        if (qEnvironmentVariableIntValue("LONGPATH_ENTWURF") != 1) {
            QSKIP("kein LONGPATH_ENTWURF=1 — Zeichenwerkzeug, kein Prüflauf");
        }
        QDir().mkpath(dir);

        // Ohne den Hausstil rendert Qt das Fenster im hellen Systemstil
        // -- ein Blatt, das so aussieht, zeigt nicht, was der Betreiber
        // vor sich hat. Derselbe Filter wie in main.cpp.
        // Dieselben drei Zeilen wie main.cpp: Fusion als Grundlage, dann
        // die dunkle Palette und das Grund-QSS. Ohne sie rendert Qt im
        // hellen Systemstil -- ein Blatt, das so aussieht, zeigt nicht,
        // was der Betreiber vor sich hat (erst beim Nachsehen gemerkt).
        qApp->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        Longpath::applyDarkPalette(*qApp);
        Longpath::applyAppBaselineQss(*qApp);
        qApp->installEventFilter(new Longpath::Style::ThemeFilter(qApp));
        Longpath::Style::Theme::instance().applyStoredChoice();

        // ── Frage 1: die fuenf wirkungslosen Regler im PS-Fenster ────
        {
            RadioModel rm;
            PsForm form(&rm, nullptr);
            form.resize(form.sizeHint());
            form.show();
            QVERIFY(QTest::qWaitForWindowExposed(&form, 5000));
            blattSpeichern(dir, "ps3-1-heute.png", form.grab().toImage(),
                           QStringLiteral("PureSignal — umgesetzt (Fassung A, mit dem echten Fenster)"),
                           QStringLiteral("Entschieden am 2026-10-21: die fünf bleiben "
                                          "sichtbar, sind abgeschaltet und tragen den "
                                          "Grund; die Zeile darunter nennt ihn. Dieses "
                                          "Blatt ist das ECHTE PsForm nach dem Umbau."));
        }
        {
            RadioModel rm;
            PsForm form(&rm, nullptr);
            for (QWidget* w : toteRegler(form)) {
                w->setEnabled(false);
                w->setToolTip(QStringLiteral(
                    "PureSignal 3 bildet die Korrekturkurve als NURB-Kurve; "
                    "diese Schraube gibt es nicht mehr."));
            }
            form.resize(form.sizeHint());
            form.show();
            QVERIFY(QTest::qWaitForWindowExposed(&form, 5000));
            blattSpeichern(dir, "ps3-2-ausgegraut.png", form.grab().toImage(),
                           QStringLiteral("Fassung A — ausgegraut, Grund im Tooltip"),
                           QStringLiteral("Die Regler bleiben sichtbar, sind aber "
                                          "abgeschaltet. Der Platz bleibt wie gewohnt; "
                                          "wer sie sucht, findet sie und liest, warum."));
        }
        {
            RadioModel rm;
            PsForm form(&rm, nullptr);
            for (QWidget* w : toteRegler(form)) { w->hide(); }
            form.resize(form.sizeHint());
            form.show();
            QVERIFY(QTest::qWaitForWindowExposed(&form, 5000));
            blattSpeichern(dir, "ps3-3-entfernt.png", form.grab().toImage(),
                           QStringLiteral("Fassung B — ganz weg"),
                           QStringLiteral("Das Fenster zeigt nur noch, was wirkt. "
                                          "Kürzer und ruhiger, aber wer die Schalter "
                                          "kennt, sucht sie einmal vergeblich."));
        }
    }

    // ── Frage 2: wo die Rotor-Weiche sitzt ──────────────────────────
    //
    // Der Rotordialog hat heute schon eine Auswahl ganz oben
    // ("Controller on this computer" / "rotctld already running"). Die
    // Frage ist, ob der neue direkte Weg dort als dritter Eintrag
    // dazukommt, als eigene Zeile daneben, oder gar nicht ins Fenster.
    //
    // Diese drei sind SKIZZEN mit der Hauspalette, nicht das echte
    // Fenster: der Dialog entsteht in einem privaten Slot des
    // Rotorfensters. Nach der Wahl wird mit dem echten Widget
    // nachgerendert, wie es die Regel verlangt.
    void rotorblaetter()
    {
        const QString dir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (dir.isEmpty() || qEnvironmentVariableIntValue("LONGPATH_ENTWURF") != 1) {
            QSKIP("kein LONGPATH_ENTWURF=1 — Zeichenwerkzeug, kein Prüflauf");
        }

        auto bauDialog = [](const QString& titel) {
            auto* dlg = new QDialog;
            dlg->setWindowTitle(QStringLiteral("Rotator"));
            auto* col = new QVBoxLayout(dlg);
            col->setContentsMargins(14, 14, 14, 14);
            col->setSpacing(10);
            auto* kopf = new QLabel(titel, dlg);
            kopf->setStyleSheet(QStringLiteral("QLabel { color: #8e9298; font-size: 11px; }"));
            col->addWidget(kopf);
            return QPair<QDialog*, QVBoxLayout*>(dlg, col);
        };

        // Fassung A: ein dritter Eintrag in der vorhandenen Auswahl.
        {
            auto [dlg, col] = bauDialog(QStringLiteral("Weg zum Rotor"));
            auto* weg = new QComboBox(dlg);
            weg->addItem(QStringLiteral("Controller on this computer"));
            weg->addItem(QStringLiteral("rotctld already running (network)"));
            weg->addItem(QStringLiteral("GS-232 direkt — ohne Hamlib"));
            weg->setCurrentIndex(2);
            col->addWidget(weg);
            auto* form = new QFormLayout;
            auto* ziel = new QLineEdit(QStringLiteral("172.30.30.16"), dlg);
            auto* port = new QLineEdit(QStringLiteral("4001"), dlg);
            form->addRow(QStringLiteral("Adresse"), ziel);
            form->addRow(QStringLiteral("Port"), port);
            auto* seriell = new QLineEdit(QStringLiteral("(leer = Netz oben)"), dlg);
            seriell->setEnabled(false);
            form->addRow(QStringLiteral("oder Schnittstelle"), seriell);
            col->addLayout(form);
            auto* knopf = new QPushButton(QStringLiteral("Verbinden"), dlg);
            col->addWidget(knopf);
            dlg->resize(dlg->sizeHint());
            dlg->show();
            QVERIFY(QTest::qWaitForWindowExposed(dlg, 5000));
            blattSpeichern(dir, "rotor-1-dritter-eintrag.png", dlg->grab().toImage(),
                           QStringLiteral("Fassung A — dritter Eintrag in der vorhandenen Auswahl"),
                           QStringLiteral("Eine Liste, drei Wege. Adresse und Schnittstelle "
                                          "stehen beide da; was nicht gilt, ist grau. "
                                          "Wenig Umbau, aber eine Zeile mehr Lesen."));
            delete dlg;
        }

        // Fassung B: zwei Fragen nacheinander — erst wer fährt, dann wie.
        {
            auto [dlg, col] = bauDialog(QStringLiteral("Weg zum Rotor"));
            auto* treiber = new QComboBox(dlg);
            treiber->addItem(QStringLiteral("Hamlib (rotctld)"));
            treiber->addItem(QStringLiteral("GS-232 direkt"));
            treiber->setCurrentIndex(1);
            col->addWidget(treiber);
            auto* form = new QFormLayout;
            auto* wie = new QComboBox(dlg);
            wie->addItem(QStringLiteral("über das Netz (ser2net, Moxa …)"));
            wie->addItem(QStringLiteral("serielle Schnittstelle"));
            form->addRow(QStringLiteral("Anschluss"), wie);
            form->addRow(QStringLiteral("Adresse"),
                         new QLineEdit(QStringLiteral("172.30.30.16:4001"), dlg));
            col->addLayout(form);
            auto* knopf = new QPushButton(QStringLiteral("Verbinden"), dlg);
            col->addWidget(knopf);
            dlg->resize(dlg->sizeHint());
            dlg->show();
            QVERIFY(QTest::qWaitForWindowExposed(dlg, 5000));
            blattSpeichern(dir, "rotor-2-zwei-fragen.png", dlg->grab().toImage(),
                           QStringLiteral("Fassung B — erst wer fährt, dann wie"),
                           QStringLiteral("Die zweite Zeile wechselt mit der ersten: bei "
                                          "Hamlib steht dort Modell und Gerät, bei GS-232 "
                                          "Netz oder Schnittstelle. Immer nur, was gilt."));
            delete dlg;
        }

        // Fassung C: gar nicht hier, sondern im Setup.
        {
            auto [dlg, col] = bauDialog(QStringLiteral("Weg zum Rotor"));
            auto* weg = new QComboBox(dlg);
            weg->addItem(QStringLiteral("Controller on this computer"));
            weg->addItem(QStringLiteral("rotctld already running (network)"));
            col->addWidget(weg);
            auto* hinweis = new QLabel(
                QStringLiteral("Der direkte GS-232-Weg steht in\n"
                               "Setup → CAT & Netz → Rotor."), dlg);
            hinweis->setStyleSheet(QStringLiteral("QLabel { color: #8e9298; font-size: 11px; }"));
            col->addWidget(hinweis);
            auto* knopf = new QPushButton(QStringLiteral("Verbinden"), dlg);
            col->addWidget(knopf);
            dlg->resize(dlg->sizeHint());
            dlg->show();
            QVERIFY(QTest::qWaitForWindowExposed(dlg, 5000));
            blattSpeichern(dir, "rotor-3-im-setup.png", dlg->grab().toImage(),
                           QStringLiteral("Fassung C — dieses Fenster bleibt, die Weiche zieht ins Setup"),
                           QStringLiteral("So macht es upstream. Das Rotorfenster bleibt "
                                          "schlank; dafür liegt die Entscheidung zwei "
                                          "Klicks weiter weg und an einer anderen Stelle "
                                          "als die Verbindung selbst."));
            delete dlg;
        }
    }
};

QTEST_MAIN(TestPs3Entwurfsblaetter)
#include "tst_ps3_entwurfsblaetter.moc"
