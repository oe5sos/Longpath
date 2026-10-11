// =================================================================
// src/core/LanDiscovery.cpp  (Longpath)
// =================================================================
//
// Longpath-native UDP listener for PowerGeniusXL / TeragenXL
// announcements on ports 9008 and 9010. Parses device model,
// IP address, version, serial, and nickname field by field (see the
// note on parseAnnouncement -- an anchored regex here cost upstream a
// disconnected tuner after a firmware update). Deduplicates by serial
// number before emitting the deviceDiscovered signal.
//
// Design reference: docs/architecture/2026-05-18-pgxl-tgxl-and-analog-smeter-plan.md (section 6.3)
// Regex pattern and wire format: FlexRadio LAN discovery protocol
//
// AI tooling: Anthropic Claude Code.

#include "LanDiscovery.h"
#include <QHash>
#include <QRegularExpression>
#include <QLoggingCategory>
#include <QHostAddress>

namespace Longpath {

Q_LOGGING_CATEGORY(lcLan, "longpath.lan")

LanDiscovery::LanDiscovery(QObject* parent) : QObject(parent) {
    connect(&m_sock9008, &QUdpSocket::readyRead, this, &LanDiscovery::on9008Ready);
    connect(&m_sock9010, &QUdpSocket::readyRead, this, &LanDiscovery::on9010Ready);
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, &LanDiscovery::onTimeout);
}

void LanDiscovery::start(int timeoutMs) {
    m_seenSerials.clear();
    m_sock9008.bind(QHostAddress::AnyIPv4, 9008,
        QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    m_sock9010.bind(QHostAddress::AnyIPv4, 9010,
        QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    m_timeout.start(timeoutMs);
}

void LanDiscovery::stop() {
    m_timeout.stop();
    m_sock9008.close();
    m_sock9010.close();
}

void LanDiscovery::on9008Ready() {
    while (m_sock9008.hasPendingDatagrams()) {
        QByteArray d(int(m_sock9008.pendingDatagramSize()), 0);
        m_sock9008.readDatagram(d.data(), d.size());
        parseAnnouncement(QString::fromUtf8(d).trimmed(), 9008);
    }
}

void LanDiscovery::on9010Ready() {
    while (m_sock9010.hasPendingDatagrams()) {
        QByteArray d(int(m_sock9010.pendingDatagramSize()), 0);
        m_sock9010.readDatagram(d.data(), d.size());
        parseAnnouncement(QString::fromUtf8(d).trimmed(), 9010);
    }
}

void LanDiscovery::onTimeout() {
    stop();
    emit scanFinished();
}

void LanDiscovery::injectDatagramForTesting(const QString& payload) {
    parseAnnouncement(payload, 9008);  // port arbitrary for tests
}

// Field-wise parse, NOT one anchored regex (changed 2026-10-11).
//
// The announcement is a model token followed by key=value fields.  The
// first version of this function matched the whole line against one
// regex anchored with ^ and $, in the field order a Tuner Genius XL on
// firmware 1.2.17 happened to send.  Any extra field broke it: the $
// stopped matching, the datagram was dropped, and the device simply
// never appeared.
//
// That is not hypothetical.  Upstream NereusSDR shipped exactly that
// bug and fixed it in the release after 2026.10.0: "A Tuner Genius XL on
// firmware 1.2.44 connects again. That firmware adds a field to the
// tuner's network announcement, which NereusSDR rejected, so the tuner
// stayed disconnected after the upgrade."  We inherited the same regex,
// so we had the same bug, and the operator has a TGXL.
//
// Reading the fields by name instead makes the parse additive: a new
// field anywhere in the line is carried past, and field order no longer
// matters.  What stays strict is what identifies the device -- a model
// token, a dotted-quad ip= and a serial= must all be present, or the
// datagram is not an announcement we can act on.
void LanDiscovery::parseAnnouncement(const QString& payload, quint16 port) {
    const QStringList tokens = payload.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (tokens.size() < 2) return;

    const QString model = tokens.first();
    if (model.contains(QLatin1Char('='))) return;   // no model token at all

    QHash<QString, QString> fields;
    for (int i = 1; i < tokens.size(); ++i) {
        const int eq = tokens.at(i).indexOf(QLatin1Char('='));
        if (eq <= 0) continue;                      // not a key=value field
        fields.insert(tokens.at(i).left(eq), tokens.at(i).mid(eq + 1));
    }

    const QString ip = fields.value(QStringLiteral("ip"));
    if (ip.isEmpty()) return;
    // Keep the dotted-quad check the old regex did: a garbage ip= must
    // still be rejected (tst_lan_discovery_regex::rejectsMalformedLine).
    static const QRegularExpression ipRx(QStringLiteral(R"(^\d+\.\d+\.\d+\.\d+$)"));
    if (!ipRx.match(ip).hasMatch()) return;

    const QString serial = fields.value(QStringLiteral("serial"));
    if (serial.isEmpty()) return;
    if (m_seenSerials.contains(serial)) return;
    m_seenSerials.insert(serial);

    if (fields.size() > 4) {
        // Worth a line in the log: it says the firmware grew fields we do
        // not read yet, which is how the 1.2.44 case announced itself.
        qCInfo(lcLan) << "LanDiscovery:" << model << "announced"
                      << fields.size() << "fields; known are ip, v, serial, nickname";
    }

    emit deviceDiscovered(model,
                          ip,
                          port,
                          fields.value(QStringLiteral("v")),
                          serial,
                          fields.value(QStringLiteral("nickname")));
}

}  // namespace Longpath
