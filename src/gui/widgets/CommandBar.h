#pragma once

// =================================================================
// src/gui/widgets/CommandBar.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// ── Die Kopfleiste ───────────────────────────────────────────────────
//
// Der größte Einzelunterschied zwischen NereusSDR und der Vorlage des
// Betreibers. Bei Zeus liegen MODE, FILTER, BAND, FAVORITES, STEP und
// FRONT-END als beschriftete Pillengruppen quer über das Fenster; hier
// stand bisher links eine Spalte senkrechter Knöpfe und der Rest
// verteilt in den Panels.
//
// Aufgabe #7 der Projektliste behauptete, so eine Leiste sei gebaut.
// Am 2026-08-15 nachgesehen: im Quellbaum existierte kein solches
// Widget. Der Eintrag war falsch.
//
// ── Was die Leiste ausmacht, und was nicht ───────────────────────────
//
// Nicht die Farben — die sind seit heute Nacht in Ordnung. Es ist die
// FORM:
//
//   · Über jeder Knopfreihe eine Versalzeile mit weiter Laufweite, in
//     Skalenfarbe. Ohne sie ist eine Reihe Pillen nur eine Reihe
//     Pillen; mit ihr ist sie eine benannte Gruppe.
//
//   · Drei sichtbare Einträge, dann ein „…". Nicht dreizehn
//     nebeneinander — das ist die Filterwand im RX-Panel, und die liest
//     niemand, der sie nicht auswendig kann.
//
//   · Genau einer je Gruppe leuchtet. Der Rest ist fast unsichtbar.
//
//   · Radius 6, Höhe 27. Radius 3 ist der Qt-Standardwert und lässt
//     alles nach Qt aussehen.
//
// ── Was bewusst fehlt ────────────────────────────────────────────────
//
// BAND, FILTER und FAVORITES sind nicht dabei. Nicht aus Faulheit: bei
// denen ist die Frage „welche drei stehen vorne" eine Entscheidung des
// Betreibers, keine des Programms. Die drei letzten Bänder? Die
// Favoriten? Die des aktuellen Antennenanschlusses?
//
// MODE und STEP haben diese Frage nicht — da gibt es eine natürliche
// Auswahl und einen vollständigen Rest im Menü. Also sind sie zuerst
// da, und die Leiste beweist ihre Form, bevor jemand über Bandlisten
// entscheiden muss.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-15 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/WdspTypes.h"

#include <QList>
#include <QPointer>
#include <QString>
#include <QVector>
#include <QWidget>

class QHBoxLayout;
class QPushButton;

namespace NereusSDR {

class SliceModel;

class CommandBar : public QWidget {
    Q_OBJECT
public:
    explicit CommandBar(QWidget* parent = nullptr);
    ~CommandBar() override;

    /// Zweiseitig an eine Empfangskette hängen. Ein zweiter Aufruf löst
    /// die vorige wieder — beim Umschalten zwischen Pans darf die
    /// Leiste nicht an der alten weiterhängen und deren Modus melden.
    void attach(SliceModel* slice);

    /// Ein Widget ans rechte Ende der Leiste hängen — dort sitzt das
    /// Plus. Getrennt vom Konstruktor, weil der Sichtbarkeitsverwalter
    /// im MainWindow erst lange nach dem Fensteraufbau entsteht und die
    /// Leiste nicht auf ihn warten soll.
    void addTrailing(QWidget* w);

    // ── Für Tests ────────────────────────────────────────────────────
    //
    // Ob die Leiste hübsch ist, kann kein Test sagen. Ob genau eine
    // Pille je Gruppe eingeschaltet ist und ob sie dem Modell folgt,
    // schon — und das sind die beiden Eigenschaften, an denen so eine
    // Leiste tatsächlich scheitert.

    /// Die Beschriftungen einer Gruppe, in Reihenfolge, ohne das „…".
    QStringList pillsIn(const QString& group) const;
    /// Die eingeschaltete Pille einer Gruppe, oder leer.
    QString activePill(const QString& group) const;
    /// Eine Pille auslösen, als hätte jemand geklickt.
    bool clickPill(const QString& group, const QString& label);
    /// Die Gruppennamen in Reihenfolge.
    QStringList groups() const;

    // Die Pillen-Optik der oberen Leiste — oeffentlich, seit die UNTERE
    // Leiste sie auch benutzt (2026-08-19, auf Ansage des Betreibers:
    // „die Taskleiste unten sollte auch wie die Taskleiste oben
    // aussehen, gleiches Design").
    //
    // Eine Funktion statt zweier Stylesheets: zwei Leisten, die
    // „gleich" aussehen sollen und ihre Farben getrennt fuehren, sehen
    // nach dem naechsten Feinschliff wieder verschieden aus.
    static QString pillStyle();

    static constexpr int kPillHeight = 27;
    static constexpr int kPillRadius = 6;
    /// Sichtbare Einträge je Gruppe, bevor das „…" übernimmt.
    static constexpr int kVisiblePerGroup = 3;

private:
    struct Group {
        QString name;
        QHBoxLayout* row{nullptr};   // wo die Pillen hineinwandern
        QVector<QPushButton*> pills;
        QPushButton* overflow{nullptr};
    };

    Group* group(const QString& name);
    const Group* group(const QString& name) const;

    /// Eine Gruppe anlegen: Überschrift, Pillen, Überlaufknopf.
    Group& addGroup(const QString& caption, QHBoxLayout* row);
    QPushButton* addPill(Group& g, const QString& label);
    /// Das „…" ans Ende einer Gruppe, mit dem vollstaendigen Rest.
    template <typename Entry, typename Apply>
    void addOverflow(Group& g, const QVector<Entry>& all, Apply apply);
    void setActive(Group& g, const QString& label);

    void buildModeGroup(QHBoxLayout* row);
    void buildStepGroup(QHBoxLayout* row);

    void pushModeToModel(DSPMode m);
    void pushStepToModel(int hz);
    void pullFromModel();

    // Zeiger, nicht Werte. addGroup() gibt eine Referenz zurueck, und
    // ein QVector<Group> verschiebt beim naechsten append() seinen
    // Speicher — die Referenz waere dann tot. Heute ginge es zufaellig
    // gut, weil keine Gruppe nach ihrer Benutzung noch eine weitere
    // anlegt; das ist keine Eigenschaft, auf die sich der naechste
    // Gruppentyp verlassen sollte.
    QVector<Group*> m_groups;
    QHBoxLayout* m_row{nullptr};   // die äußere Reihe, für addTrailing()
    QPointer<SliceModel> m_slice;
    QList<QMetaObject::Connection> m_links;

    // Der vollständige Rest, den das „…" anbietet.
    QVector<QPair<QString, DSPMode>> m_allModes;
    QVector<QPair<QString, int>> m_allSteps;
};

} // namespace NereusSDR
