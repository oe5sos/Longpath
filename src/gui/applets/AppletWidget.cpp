// =================================================================
// src/gui/applets/AppletWidget.cpp  (Longpath)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   Longpath is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (Longpath):
//   2026-04-18 — Implemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Shared applet base class. Title-bar gradient, slider-row,
//                 and toggle-button helpers extracted from the AetherSDR
//                 `src/gui/AppletPanel.{h,cpp}` styling pattern (already
//                 registered for AppletPanelWidget in Bucket A); this base
//                 class hoists that styling up so every Longpath applet
//                 (Cat/Cwx/Dvk/Tuner/Eq/Fm/Tx/Rx/PhoneCw/Diversity/Digital/
//                 PureSignal) inherits identical visuals without
//                 duplicating the code.
// =================================================================

#include "AppletWidget.h"
#include "models/RadioModel.h"   // vollstaendiger Typ fuer QPointer<RadioModel>
#include "gui/StyleConstants.h"
#include "gui/ComboStyle.h"

#include "core/AppSettings.h"
#include "models/SliceModel.h"

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QSlider>
#include <QFrame>
#include <QHBoxLayout>

namespace Longpath {

AppletWidget::AppletWidget(RadioModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
{
    // ── Die Schriftgroesse gehoert NICHT in dieses Stylesheet ────────
    //
    // Hier stand `QLabel { color: %1; font-size: 11px; }`. Ein
    // Qt-Stylesheet kaskadiert auf JEDES untergeordnete Label und
    // GEWINNT gegen setFont(). Damit war die gesamte Schriftleiter
    // innerhalb der Applets ausser Kraft: jedes Instrument, das seine
    // Groesse per setFont() setzt, wurde auf 11 px gezogen.
    //
    // Sichtbar geworden an der Frequenzanzeige (Betreiber, 2026-08-18).
    // FrequencyInstrument berechnet die feste Breite jeder Ziffer aus
    // der Zelle der 38-px-Ziffernschrift und zeichnete die Glyphe dann
    // mit 11 px — jedes Schild dreieinhalbmal so breit wie sein
    // Zeichen. Die Zeile zerfiel in gleich weit stehende Zeichen
    // („7 . 1 3 1 . 3 0 0"), obwohl die drei Abstandskonstanten in
    // FrequencyInstrument.cpp genau das verhindern sollen.
    //
    // Die Farbe bleibt im Stylesheet — sie soll kaskadieren. Die
    // Groesse kommt per setFont(): die erbt genauso an alle Kinder,
    // aber ein Kind mit eigenem setFont() behaelt seines.
    setFont([this] {
        QFont f = font();
        f.setPixelSize(Style::kFontSmall);
        return f;
    }());
    setStyleSheet(QStringLiteral(
        "QLabel { color: %1; }"
    ).arg(Style::kTextPrimary)
    + Style::buttonBaseStyle()
    + Style::sliderHStyle());
}

QIcon AppletWidget::appletIcon() const { return {}; }

// --- Empfaengerbindung ---------------------------------------------------

namespace {
/// Wo die Wahl gemerkt wird. Je Applet eine Zeile.
QString receiverKey(const QString& appletId)
{
    return QStringLiteral("Applet/%1/Receiver").arg(appletId);
}
} // namespace

void AppletWidget::initReceiverBinding()
{
    m_receiverChoice = AppSettings::instance()
                           .value(receiverKey(appletId()), kFollowsActive)
                           .toInt();

    if (m_model) {
        // Folgen wir dem aktiven, muessen wir bei jedem Wechsel neu
        // aufloesen. Haengen wir fest, aendert ein Wechsel nichts -- die
        // Pruefung in refreshBoundSlice merkt das und meldet nichts.
        connect(m_model, &RadioModel::activeSliceChanged,
                this, [this](int) { refreshBoundSlice(); });
        // Verschwindet der Empfaenger, auf den wir zeigen, faellt
        // boundSlice() auf nullptr -- auch das ist eine Aenderung.
        connect(m_model, &RadioModel::sliceRemoved,
                this, [this](int) { refreshBoundSlice(); });
        // Und andersherum: wer RX2 waehlt, BEVOR es RX2 gibt, zeigt auf
        // nullptr. Kommt der Empfaenger spaeter dazu, muss die Bindung
        // nachziehen -- sonst bleibt das Applet stumm, obwohl sein
        // Empfaenger laengst da ist, und man sucht den Fehler im
        // Decoder.
        connect(m_model, &RadioModel::sliceAdded,
                this, [this](int) { refreshBoundSlice(); });
    }
    m_lastBound = boundSlice();
    // Den Zusatz hier noch einmal setzen, nicht nur in appletTitleBar():
    // die abgeleiteten Klassen bauen ihre Oberflaeche teils VOR diesem
    // Aufruf (CwDecoderApplet ruft buildUI() als erstes im Konstruktor).
    // Ohne das stuende nach dem Start "CW DECODER" in der Leiste, obwohl
    // ein fester Empfaenger gewaehlt ist, und der Zusatz kaeme erst beim
    // naechsten Wechsel. setTitleBarSuffix tut nichts, solange es die
    // Leiste noch nicht gibt -- die Reihenfolge ist damit egal.
    setTitleBarSuffix(receiverChoiceLabel());
}

void AppletWidget::setReceiverChoice(int sliceId)
{
    if (m_receiverChoice == sliceId) {
        return;
    }
    m_receiverChoice = sliceId;
    AppSettings::instance().setValue(receiverKey(appletId()), sliceId);
    setTitleBarSuffix(receiverChoiceLabel());
    refreshBoundSlice();
}

SliceModel* AppletWidget::boundSlice() const
{
    if (!m_model) {
        return nullptr;
    }
    if (m_receiverChoice == kFollowsActive) {
        return m_model->activeSlice();
    }
    // Die Wahl ist eine KENNUNG, keine Listenposition. Mit einer
    // Position haengt das Applet nach dem Loeschen eines Empfaengers
    // ploetzlich an einem anderen Geraet -- die Ueberlebenden ruecken
    // nach, "RX2" meint dann jemand anderen. sliceById liefert nullptr,
    // wenn es den Empfaenger nicht mehr gibt, und genau das ist richtig.
    return m_model->sliceById(m_receiverChoice);
}

QString AppletWidget::receiverChoiceLabel() const
{
    if (m_receiverChoice == kFollowsActive) {
        return {};
    }
    // Die Beschriftung zaehlt Positionen ("RX2" ist der zweite in der
    // Liste), die Wahl selbst ist eine Kennung. Gibt es den Empfaenger
    // nicht mehr, sagt die Leiste das -- besser als eine Zahl, die auf
    // nichts zeigt.
    if (!m_model) {
        return {};
    }
    const QList<SliceModel*> liste = m_model->slices();
    for (int i = 0; i < liste.size(); ++i) {
        if (liste.at(i) && liste.at(i)->sliceIndex() == m_receiverChoice) {
            return QStringLiteral("RX%1").arg(i + 1);
        }
    }
    return QStringLiteral("RX? (fehlt)");
}

void AppletWidget::refreshBoundSlice()
{
    // Die Beschriftung ZUERST, und vor der Pruefung unten: sie rechnet
    // sich aus der Listenposition, und die aendert sich auch dann, wenn
    // die Bindung selbst gleich bleibt. Faellt RX1 weg, heisst der
    // gewaehlte Empfaenger ploetzlich RX1 statt RX2 -- der gebundene
    // Zeiger ist derselbe, die Leiste zeigte sonst weiter die alte
    // Nummer. (Vom Pruefstand gefangen:
    // dieWahlHaengtAmGeraetNichtAmListenplatz.)
    setTitleBarSuffix(receiverChoiceLabel());

    SliceModel* jetzt = boundSlice();
    if (jetzt == m_lastBound) {
        return;
    }
    m_lastBound = jetzt;
    emit boundSliceChanged(jetzt);
}

QWidget* AppletWidget::receiverChoiceWidget(QWidget* parent)
{
    auto* box = new QWidget(parent);
    auto* vbox = new QVBoxLayout(box);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(4);

    auto* titel = new QLabel(QStringLiteral("Empfänger"), box);
    // Schriftstufen aus der Leiter in StyleConstants.h -- eine eigene
    // Groesse dazwischen faellt in scripts/verify-style-drift.py.
    titel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: %2px; }")
                             .arg(QLatin1String(Style::kTextTertiary))
                             .arg(Style::kFontCaption));
    vbox->addWidget(titel);

    auto* waehler = new QComboBox(box);
    waehler->setObjectName(QStringLiteral("receiverChoiceCmb"));
    waehler->addItem(QStringLiteral("folgt dem aktiven"), kFollowsActive);
    const QList<SliceModel*> liste = m_model ? m_model->slices() : QList<SliceModel*>{};
    bool wahlDabei = (m_receiverChoice == kFollowsActive);
    for (int i = 0; i < liste.size(); ++i) {
        SliceModel* sm = liste.at(i);
        if (!sm) { continue; }
        // Beschriftung nach Position, Wert ist die Kennung.
        waehler->addItem(QStringLiteral("RX%1").arg(i + 1), sm->sliceIndex());
        if (sm->sliceIndex() == m_receiverChoice) { wahlDabei = true; }
    }
    // Zeigt die Wahl auf einen Empfaenger, den es nicht mehr gibt, steht
    // sie trotzdem in der Liste -- sonst faende man nicht mehr heraus,
    // worauf das Applet wartet.
    if (!wahlDabei) {
        waehler->addItem(QStringLiteral("RX? (fehlt)"), m_receiverChoice);
    }
    const int idx = waehler->findData(m_receiverChoice);
    if (idx >= 0) { waehler->setCurrentIndex(idx); }

    connect(waehler, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this, waehler](int i) {
        if (i < 0) { return; }
        setReceiverChoice(waehler->itemData(i).toInt());
    });
    vbox->addWidget(waehler);

    auto* hinweis = new QLabel(
        QStringLiteral("Fest gewählt bleibt dieses Fenster an seinem "
                       "Empfänger, auch wenn woanders gearbeitet wird."), box);
    hinweis->setWordWrap(true);
    hinweis->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: %2px; }")
                               .arg(QLatin1String(Style::kTextTertiary))
                               .arg(Style::kFontSmall));
    vbox->addWidget(hinweis);

    return box;
}

void AppletWidget::setTitleBarSuffix(const QString& suffix)
{
    if (!m_titleBarLabel) {
        return;
    }
    m_titleBarLabel->setText(suffix.isEmpty()
                                 ? m_titleBarText
                                 : QStringLiteral("%1 · %2").arg(m_titleBarText, suffix));
}

QWidget* AppletWidget::appletTitleBar(const QString& text)
{
    auto* bar = new QWidget(this);
    bar->setFixedHeight(Style::kTitleBarH);
    bar->setStyleSheet(Style::titleBarStyle());

    auto* hbox = new QHBoxLayout(bar);
    hbox->setContentsMargins(2, 0, 4, 0);
    hbox->setSpacing(4);

    auto* grip = new QLabel(QStringLiteral("\u22EE\u22EE"), bar);
    grip->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; background: transparent; }"
    ).arg(Style::kTextScale));
    hbox->addWidget(grip);

    auto* label = new QLabel(text, bar);
    label->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; font-weight: bold;"
        " background: transparent; }"
    ).arg(Style::kTitleText));
    hbox->addWidget(label);
    // Merken, damit der Empfaenger-Zusatz spaeter dazukann.
    m_titleBarText  = text;
    m_titleBarLabel = label;
    setTitleBarSuffix(receiverChoiceLabel());
    hbox->addStretch();

    return bar;
}

QHBoxLayout* AppletWidget::sliderRow(const QString& labelText,
                                      QSlider* slider, QLabel* valueLabel,
                                      int labelWidth)
{
    auto* row = new QHBoxLayout;
    row->setSpacing(4);

    auto* lbl = new QLabel(labelText, this);
    lbl->setFixedWidth(labelWidth);
    lbl->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; }").arg(Style::kTextSecondary));
    row->addWidget(lbl);

    slider->setFixedHeight(18);
    row->addWidget(slider, 1);

    if (valueLabel) {
        // Mindestbreite, nicht feste Breite: der Wertchip ist seit dem
        // 2026-09-17 Monospace und breiter — "50 dB" stand als "50 d"
        // auf dem Phone/CW-Blatt. Waechst mit seinem Text.
        valueLabel->setMinimumWidth(36);
        valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        valueLabel->setStyleSheet(Style::insetValueStyle());
        row->addWidget(valueLabel);
    }

    return row;
}

QPushButton* AppletWidget::styledButton(const QString& text, int w, int h)
{
    auto* btn = new QPushButton(text, this);
    btn->setCheckable(true);
    btn->setFixedHeight(h);
    // Nie schmaler als sein Text: die gewuenschte Breite ist ein
    // Mindestmass. Auf den Applet-Blaettern vom 2026-09-17 standen
    // "MUTI", "ROC", "DEXP" ohne Anfang und "/AC 1" — feste Breiten aus
    // der Zeit vor der breiteren Knopfpolsterung des Hausstils.
    if (w > 0) {
        btn->ensurePolished();
        btn->setFixedWidth(qMax(w, btn->sizeHint().width()));
    }
    return btn;
}

QPushButton* AppletWidget::greenToggle(const QString& text, int w, int h)
{
    auto* btn = styledButton(text, w, h);
    btn->setStyleSheet(btn->styleSheet() + Style::greenCheckedStyle());
    return btn;
}

QPushButton* AppletWidget::blueToggle(const QString& text, int w, int h)
{
    auto* btn = styledButton(text, w, h);
    btn->setStyleSheet(btn->styleSheet() + Style::blueCheckedStyle());
    return btn;
}

QPushButton* AppletWidget::amberToggle(const QString& text, int w, int h)
{
    auto* btn = styledButton(text, w, h);
    btn->setStyleSheet(btn->styleSheet() + Style::amberCheckedStyle());
    return btn;
}

QLabel* AppletWidget::insetValue(const QString& text, int w)
{
    auto* lbl = new QLabel(text, this);
    lbl->setFixedWidth(w);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet(Style::insetValueStyle());
    return lbl;
}

QFrame* AppletWidget::divider()
{
    // Eine Rille statt Qt's HLine (2026-09-18): Fusion malte die
    // "Sunken"-Linie aus der Palette hell — auf dem CAT-Blatt ein
    // weisser Strich. Jetzt zwei Pixel wie jede Kante im Haus:
    // oben Schatten, unten Licht.
    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::NoFrame);
    line->setFixedHeight(2);
    line->setStyleSheet(QStringLiteral(
        "QFrame { border: none; background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        " stop:0 %1, stop:0.5 %1, stop:0.51 %2, stop:1 %2); }")
        .arg(QLatin1String(Style::kGlassShade), QLatin1String(Style::kGlassLight)));
    return line;
}

} // namespace Longpath
