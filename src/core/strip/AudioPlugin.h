#pragma once

// =================================================================
// src/core/strip/AudioPlugin.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Ein fremdes Effektgeraet in unserer Kette -- als ZUSATZ, nie als
// Weg.
//
// Die Ansage des Betreibers am 2026-10-09, und sie steht hier, damit
// sie niemand aufweicht: „will das unabhaengig machen [...] plugins
// koennen nur als option zusaetzlich gelten, wenn gewollt. ist nicht
// der standard."
//
// Daraus folgen drei Regeln, die jede spaetere Aenderung einhalten
// muss:
//
//   1. Leer ist der Normalfall. Wer nie einen Einschub befuellt,
//      merkt von alledem nichts -- kein Suchlauf beim Start, kein
//      Reiter, der sich aufdraengt, keine Rechenzeit. Die acht
//      eigenen Stufen bleiben, was Longpath kann.
//   2. Ein fehlendes Plugin ist kein Fehler. Wird eine Einstellung
//      auf einem Rechner geoeffnet, auf dem das Plugin nicht
//      installiert ist, bleibt der Einschub eben leer und die Kette
//      klingt wie ohne ihn. Keine Warnung beim Start, keine Sperre.
//   3. Fremder Code steht unter demselben Verdacht wie der Mikrofon-
//      eingang. StripChain prueft nach jeder Stufe auf Werte, die
//      keine Zahlen sind, und setzt die Stufe zurueck, wenn sie
//      welche liefert. Fuer ein Plugin gilt das erst recht: es ist
//      das einzige Stueck in dieser Kette, das wir nicht gelesen
//      haben.
//
// Diese Schnittstelle ist bewusst winzig. Sie kennt keine Fenster und
// keine Zeichenflaechen: die Parameter werden ausgelesen und in
// unseren eigenen Reglern dargestellt. Das ist kein Notbehelf,
// sondern die Entscheidung -- es haelt den Hausstil, es laeuft
// plattformgleich, und es umgeht das Einbetten fremder Ansichten in
// Qt, an dem solche Vorhaben ueblicherweise scheitern. Der Preis ist
// ehrlich zu nennen: ein Plugin, dessen Koennen in seiner Oberflaeche
// steckt -- ein gezeichneter Entzerrerverlauf etwa --, ist hier eine
// Liste von Zahlen.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-09 — Created in C++20/Qt6, AI-assisted via Anthropic
//                 Claude (Claude Code), operator Martin Fischer.
// =================================================================

#include <QByteArray>
#include <QString>
#include <QVector>

#include <memory>

namespace Longpath {

// Was auf diesem Rechner zu haben ist. Der Bezeichner ist das, was in
// die Einstellungen geschrieben wird; er muss einen Neustart und ein
// Programmupdate ueberleben, also ist es nicht der Anzeigename.
struct PluginDescriptor {
    QString identifier;    // z. B. "au:aufx:dely:appl"
    QString name;          // "AUDelay"
    QString vendor;        // "Apple"
    QString format;        // "AU"
};

// Ein Regler des Plugins, so wie es ihn selbst beschreibt.
struct PluginParameter {
    quint32 id{0};
    QString name;
    QString unit;          // leer, wenn das Plugin keine nennt
    float   minValue{0.0f};
    float   maxValue{1.0f};
    float   defaultValue{0.0f};
    bool    isSwitch{false};   // an/aus statt Regler
};

class AudioPlugin {
public:
    virtual ~AudioPlugin() = default;

    virtual PluginDescriptor descriptor() const = 0;

    // Hauptfaden. Vor dem ersten process(), und bei jeder
    // Ratenaenderung. false heisst: dieses Plugin laeuft hier nicht
    // (falsche Kanalzahl, abgelehnte Rate) -- der Einschub bleibt
    // dann leer, und das ist kein Fehler.
    virtual bool prepare(double sampleRate, int maxBlockFrames) = 0;

    // Audiofaden. Einkanalig, an Ort und Stelle. Darf blockieren so
    // wenig wie jede andere Stufe -- was ein fremdes Plugin daraus
    // macht, koennen wir nicht erzwingen, nur ueberwachen.
    virtual void processMono(float* samples, int frames) noexcept = 0;

    virtual void reset() noexcept = 0;

    virtual QVector<PluginParameter> parameters() const = 0;
    virtual float parameterValue(quint32 id) const = 0;
    virtual void  setParameterValue(quint32 id, float value) = 0;

    // Der ganze Zustand des Plugins, zum Mitspeichern im Profil.
    // Leer, wenn das Plugin keinen hergibt -- dann werden beim
    // Wiederherstellen die einzelnen Parameter gesetzt, was fuer fast
    // alles reicht.
    virtual QByteArray saveState() const { return {}; }
    virtual bool restoreState(const QByteArray&) { return false; }

    // ── Die eigene Oberflaeche des Plugins ──────────────────────────
    //
    // Nachgereicht am 2026-10-09: „haette gehofft, dass die plugins
    // ev. graphisch vorhanden sind." Der Einwand ist berechtigt --
    // bei einem Entzerrer steckt das halbe Werkzeug in der
    // Zeichnung.
    //
    // Die Entscheidung oben bleibt trotzdem stehen: unsere eigenen
    // Regler sind nicht das schlechtere Fenster, sondern das, was
    // auch dann noch da ist, wenn ein Plugin keine Oberflaeche
    // mitbringt -- und das Einzige, was spaeter auf Windows und
    // Linux gleich aussieht. Das Fenster kommt DANEBEN, nicht an
    // ihre Stelle.
    virtual bool hasEditor() const { return false; }
    virtual bool showEditor(const QString& /*title*/) { return false; }
    virtual void closeEditor() {}

    // Wie viele Rahmen Verzoegerung das Plugin meldet. Beim Senden
    // belanglos, beim Mithoeren nicht -- deshalb wird es angezeigt.
    virtual int latencyFrames() const { return 0; }
};

// Der Suchlauf. Laeuft NICHT beim Programmstart, sondern erst, wenn
// jemand die Einschubseite aufmacht oder ein gespeichertes Plugin
// gesucht wird (Regel 1).
namespace PluginHost {

// Alle Effekte, die dieser Rechner anbietet, nach Namen sortiert.
// Auf Windows und Linux heute leer: dort fehlt der Wirt noch, und
// eine leere Liste ist die ehrliche Antwort darauf.
QVector<PluginDescriptor> scan();

// Lade eines. Gibt nullptr zurueck, wenn es nicht da ist -- siehe
// Regel 2, das ist der Normalfall auf einem anderen Rechner.
std::unique_ptr<AudioPlugin> load(const QString& identifier);

// Ob auf dieser Plattform ueberhaupt ein Wirt eingebaut ist. Die
// Oberflaeche sagt damit „hier gibt es das nicht" statt „hier ist
// nichts installiert" -- zwei verschiedene Tatsachen.
bool available();

} // namespace PluginHost

} // namespace Longpath
