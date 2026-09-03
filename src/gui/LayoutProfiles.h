#pragma once

// =================================================================
// src/gui/LayoutProfiles.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// ── Mehrere Ansichten, jede selbst gestaltet ─────────────────────────
//
// OE5SOS, 2026-08-15:
//
//   „Wenn dies alles erledigt ist, soll die Möglichkeit entstehen, ein
//    zweites Profil oder auch ein drittes anzulegen, welches ich mir
//    selbst wieder individuell gestalten kann."
//
// Und davor, zum Zweck:
//
//   „Sodass man je nach Band oder je nach Modus andere Ansichten hat."
//
// Bei Zeus sind das die runden Abzeichen links in der Schiene — D, W,
// A, L — mit einem gestrichelten Plus darunter. Jedes ist eine
// Arbeitsfläche.
//
// ── Was es dafür schon gab, und was nicht ────────────────────────────
//
// ContainerManager kann saveState() und restoreState(). Genau EINEN
// Zustand: den letzten. Aufgabe #3 der Projektliste behauptete
// „Layout-Profile" seien erledigt — nachgesehen am 2026-08-15: das
// Anordnen und Ausblenden gibt es, benannte Profile nicht, und im
// ganzen Projekt existiert kein Schlüssel dafür. Der zweite falsche
// Eintrag an einem Tag.
//
// ── Warum diese Klasse nichts über Container weiß ────────────────────
//
// Ein Profil ist hier eine benannte Momentaufnahme aus zwei Haken:
// einer, der den aktuellen Zustand einsammelt, und einer, der ihn
// wieder herstellt. Was in der Aufnahme steht, entscheidet der Aufrufer.
//
// Das ist kein Selbstzweck. Ein Profilverwalter, der ContainerManager,
// AppletVisibilityController und die Fenstergeometrie kennt, ist ohne
// laufende Oberfläche nicht prüfbar — und dann sind genau die Fälle
// ungetestet, die weh tun: das Profil, das beim Umschalten die Hälfte
// vergisst, und das gelöschte, das beim Neustart wiederkommt.
//
// ── Bindung an Band und Modus ────────────────────────────────────────
//
// Ein Profil kann an Bänder und Betriebsarten gebunden werden. Wer auf
// 40 m in CW geht, bekommt sein CW-Fenster, ohne es zu wählen.
//
// Die Bindung ist bewusst kein Automatismus mit Vorrangregeln: passt
// mehr als ein Profil, gewinnt das zuerst angelegte, und das steht auch
// so in profileFor(). Eine Heuristik, die „meistens das richtige"
// wählt, ist bei einem Fenster, in dem man sendet, schlechter als eine
// Regel, die man vorhersagen kann.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-15 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/WdspTypes.h"
#include "models/Band.h"

#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <functional>
#include <optional>

namespace Longpath {

class LayoutProfiles : public QObject {
    Q_OBJECT
public:
    explicit LayoutProfiles(QObject* parent = nullptr);

    /// Wie ein Zustand eingesammelt und wieder hergestellt wird. Ohne
    /// Haken verwaltet die Klasse leere Aufnahmen — brauchbar für
    /// Tests, nutzlos im Betrieb, und beides absichtlich.
    using Capture = std::function<QVariantMap()>;
    using Apply   = std::function<void(const QVariantMap&)>;
    void setHooks(Capture capture, Apply apply);

    // ── Verwalten ────────────────────────────────────────────────────

    /// Profilnamen in Anlegereihenfolge.
    QStringList names() const { return m_order; }
    QString current() const { return m_current; }
    bool exists(const QString& name) const;

    /// Neues Profil aus dem AKTUELLEN Zustand — „Speichern unter".
    /// Sichert vorher das laufende Fenster ins bisher aktive Profil und
    /// wechselt danach in das neue. Scheitert bei leerem oder schon
    /// vergebenem Namen.
    ///
    /// Das Wechseln ist nicht Bequemlichkeit, sondern Notwendigkeit:
    /// bliebe das alte Profil aktiv, schriebe das nächste Umschalten
    /// den Aufbau des neuen in das alte.
    bool create(const QString& name);

    /// Neues Profil mit einem VORGEGEBENEN Zustand statt einer Aufnahme
    /// des jetzigen. Sichert vorher genauso, wechselt hinüber und stellt
    /// den gegebenen Zustand her.
    ///
    /// Dafür gibt es einen Grund, und er kam vom Betreiber
    /// (2026-08-15): „Wenn ich links ein neues Profil öffne, sollte
    /// dieses leer sein." Ein Plus, das eine Kopie anlegt, ist ein
    /// Duplizieren-Knopf mit falscher Beschriftung — und Duplizieren
    /// gibt es schon, im Rechtsklickmenü.
    ///
    /// Was „leer" heißt, entscheidet der Aufrufer. Diese Klasse weiß
    /// nach wie vor nicht, was in einer Aufnahme steht.
    bool createWith(const QString& name, const QVariantMap& state);

    /// Kopie eines bestehenden Profils — der übliche Weg zu einem
    /// zweiten: erst so übernehmen, dann umbauen.
    bool duplicate(const QString& from, const QString& to);

    bool rename(const QString& from, const QString& to);
    bool remove(const QString& name);

    /// Umschalten. Sichert vorher den aktuellen Zustand ins bisher
    /// aktive Profil — sonst geht jede Umgestaltung verloren, sobald
    /// jemand ein anderes Profil anklickt, und das ist ein Verlust, der
    /// nicht rückgängig zu machen ist.
    bool activate(const QString& name);

    /// Den aktuellen Zustand ins aktive Profil schreiben, ohne zu
    /// wechseln.
    void captureIntoCurrent();

    /// m_apply für das aktuell gesetzte Profil auslösen, auch wenn es
    /// schon "aktiv" ist. Für den Start gedacht: load() setzt m_current
    /// direkt (ohne m_apply aufzurufen), und activate(current()) würde
    /// an der Namensgleich-Wache in activate() sofort zurückkehren —
    /// die Umgestaltung (schwebende Fenster, Splitter, Rotor-Sichtbarkeit,
    /// Container-Geometrie) käme nie an. Bug gefunden 2026-08-28
    /// (Betreiber: "und schon wieder hat er mein layout nicht
    /// gespeichert" — es wurde gespeichert, nur nie wieder angewandt).
    ///
    /// NUR für diesen Start-Fall gedacht, nicht als allgemeiner Ersatz
    /// für activate(): es fehlt absichtlich captureIntoCurrent() (kein
    /// ANDERES Profil wird verlassen, es gibt nichts zu sichern) —
    /// wer diese Methode fuer einen Fall aufruft, in dem tatsaechlich
    /// ungespeicherte Aenderungen an einem WECHSELNDEN Profil haengen,
    /// verliert sie stillschweigend. Für einen echten Wechsel activate()
    /// verwenden.
    void applyCurrent();

    QVariantMap snapshot(const QString& name) const;

    /// Ein einzelnes Profil (Name, Zustand, Band-/Modus-Bindung) als
    /// eigenstaendiges JSON -- dieselbe Feldform wie ein Eintrag in
    /// save()s Array, nur einzeln herausgeloest, fuer eine manuelle
    /// Sicherung auf den Schreibtisch. Leeres Array bei unbekanntem Namen.
    QByteArray exportToJson(const QString& name) const;

    /// Das Gegenstueck: ein von exportToJson() geschriebenes JSON in ein
    /// BESTEHENDES Profil einspielen -- ersetzt dessen Zustand samt
    /// Band-/Modus-Bindung. Der eingebettete Name aus der Datei wird
    /// dabei ignoriert; welches Profil ueberschrieben wird, entscheidet
    /// targetProfile (bei Longpath: der Rechtsklick-Ort, nicht die
    /// Datei).
    ///
    /// Betreiber, 2026-08-30: „wenn ich importiere will ich es nicht
    /// als neues profil importiren" -- ein frueherer Anlauf legte
    /// stattdessen immer ein neues Profil an ("Buero (2)"), aus Sorge
    /// vor stillem Datenverlust. Das war die falsche Sorge: ein Import
    /// IST ein Wiederherstellen, und ein Wiederherstellen, das nichts
    /// ersetzt, stellt nichts wieder her. Die Absicherung gegen
    /// versehentliches Ueberschreiben gehoert an die Aufrufstelle (ein
    /// Rueckfragedialog), nicht in diese Klasse.
    ///
    /// Ist targetProfile das gerade AKTIVE Profil, wird der neue
    /// Zustand sofort angewendet (m_apply) -- sonst erst beim naechsten
    /// Umschalten dorthin sichtbar. false bei kaputtem/unlesbarem JSON
    /// oder unbekanntem targetProfile; outError traegt dann eine fuer
    /// den Betreiber lesbare Begruendung.
    bool importFromJson(const QString& targetProfile, const QByteArray& json,
                        QString* outError = nullptr);

    // ── Bindung ──────────────────────────────────────────────────────

    void bindBand(const QString& name, Band b, bool on);
    void bindMode(const QString& name, DSPMode m, bool on);
    bool isBoundToBand(const QString& name, Band b) const;
    bool isBoundToMode(const QString& name, DSPMode m) const;

    /// Welches Profil zu Band und Modus passt, oder leer. Passt mehr
    /// als eines, gewinnt das zuerst angelegte.
    ///
    /// Ein Profil ohne jede Bindung passt NIE automatisch — sonst
    /// würde das erste, das jemand anlegt, jeden Bandwechsel an sich
    /// reißen.
    QString profileFor(Band b, DSPMode m) const;

    // ── Platte ───────────────────────────────────────────────────────
    void save() const;
    void load();
    static QString settingsKey();

signals:
    void profilesChanged();
    void currentChanged(const QString& name);

private:
    struct Profile {
        QString name;
        QVariantMap state;
        QSet<int> bands;
        QSet<int> modes;
    };

    /// Der gemeinsame Weg hinter create() und createWith(). Ohne
    /// Vorgabe wird der jetzige Zustand aufgenommen, mit Vorgabe wird
    /// sie hergestellt — sonst identisch, damit die Regel „erst
    /// sichern, dann anlegen, dann hinüberwechseln" an einer Stelle
    /// steht und nicht an zweien auseinanderlaufen kann.
    bool createInternal(const QString& name,
                        const std::optional<QVariantMap>& given);

    Profile* find(const QString& name);
    const Profile* find(const QString& name) const;

    /// Ruft m_apply() und haelt m_applyingState waehrenddessen hoch --
    /// der EINZIGE Weg, wie m_apply je aufgerufen wird. Siehe
    /// m_applyingState fuer den Grund.
    void runApply(const QVariantMap& state);

    QList<Profile> m_profiles;
    QStringList m_order;
    QString m_current;
    Capture m_capture;
    Apply m_apply;

    // Coalesces the disk flush inside save() (Review-Fund 2026-08-28) --
    // same bool-guard + QTimer::singleShot(500, ...) idiom as
    // RadioModel::scheduleSettingsSave(). See save()'s definition for why
    // this is safe: MainWindow::closeEvent's own unconditional
    // AppSettings::instance().save() at quit is the backstop.
    mutable bool m_diskFlushScheduled{false};

    // Betreiber 2026-08-31, nach sehr langer Suche: "das speichern und
    // wieder aufrufen hat bis dato nicht geklappt" / "es passt gar
    // nichts" / "Fensteranordnung falsch" nach einem Profilwechsel oder
    // -import. Ursache: m_apply() haengt schwebende Fenster um, und
    // MINDESTENS einer dieser Umhaenge-Schritte
    // (MainWindow::dockAppletBack(), aufgerufen fuer jedes Applet, das
    // im NEUEN Profil nicht mehr abgeloest sein soll) ruft SELBST wieder
    // captureIntoCurrent()+save() -- REENTRANT, WAEHREND m_apply() fuer
    // GENAU DIESES Profil noch mitten in der Anwendung steht. Die
    // Aufnahme dabei ist der halb angewendete, innerlich inkonsistente
    // Zwischenstand (manche Schluessel schon uebernommen, Container-
    // Geometrie/Splitter/Rotor/Vollbild noch nicht) -- und ueberschreibt
    // damit sofort wieder, was gerade erst importiert/hergestellt wurde.
    // Fast identisch mit dem Fund, der m_shuttingDown in
    // dockAppletBack() begruendet (siehe dort) -- nur dass DIESER Pfad
    // nicht beim Beenden, sondern bei JEDEM Profilwechsel/-import
    // zuschlagen kann, der ein abgeloestes Fenster zurueckholt.
    // runApply() haelt dieses Flag waehrend jedes m_apply()-Aufrufs
    // hoch; captureIntoCurrent() wird waehrenddessen zum No-Op.
    bool m_applyingState{false};
};

} // namespace Longpath
