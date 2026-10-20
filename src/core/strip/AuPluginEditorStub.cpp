// =================================================================
// src/core/strip/AuPluginEditorStub.cpp  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Die Gegenseite von AuPluginEditor.mm fuer alles, was kein macOS
// ist. AuPluginEditor.h verspricht die drei Rufe plattformweit --
// „Gibt es auf dieser Plattform ueberhaupt Fenster dieser Art?"
// steht dort ueber available() --, gebaut wurde bisher aber nur die
// Cocoa-Haelfte (CMakeLists.txt: list(APPEND CORE_SOURCES ...) nur
// unter if(APPLE)). Wer die Rufe also ausserhalb von macOS benutzt,
// bekam sie erst beim Linken um die Ohren; tst_strip_plugin_inserts
// fiel am 2026-10-20 genau so aus:
//
//     mold: error: undefined symbol: Longpath::AuPluginEditor::close(void*)
//
// Ein Wachter an jeder Rufstelle waere der andere Weg gewesen. Dieser
// hier ist der, den die Kopfdatei vorzeichnet: available() sagt nein,
// und alles Weitere ist damit beantwortet. Audio Units gibt es nur auf
// macOS; auf Windows und Linux bleiben unsere eigenen Regler die
// Bedienung, wie es im Kopf von AuPluginEditor.h schon steht.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-20 — Created in C++20 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Claude Code).
// =================================================================

#include "core/strip/AuPluginEditor.h"

namespace Longpath::AuPluginEditor {

bool available()
{
    return false;
}

bool show(void* /*unit*/, const QString& /*title*/)
{
    // Kein Fehlerfall -- siehe AuPluginEditor.h: false heisst „keine
    // eigene Ansicht", und die eigenen Regler bleiben die Bedienung.
    return false;
}

void close(void* /*unit*/)
{
}

} // namespace Longpath::AuPluginEditor
