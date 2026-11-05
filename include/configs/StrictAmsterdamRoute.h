#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QFileInfo>
#include <map>

// Opt-in isolated profile: route through pinned native SSH.
namespace Configs::StrictAmsterdamRoute {
inline bool tailscaleAddress(const QString& host) {
    const auto parts = host.split('.');
    if (parts.size() != 4) return false;
    int octets[4];
    for (int i = 0; i < 4; ++i) {
        bool ok = false;
        octets[i] = parts[i].toInt(&ok);
        if (!ok || octets[i] < 0 || octets[i] > 255 || QString::number(octets[i]) != parts[i]) return false;
    }
    return octets[0] == 100 && octets[1] >= 64 && octets[1] <= 127;
}

inline bool pinnedSsh(const QJsonObject& outbound) {
    const auto keys = outbound.value("host_key").toArray();
    const auto algorithms = outbound.value("host_key_algorithms").toArray();
    const auto port = outbound.value("server_port").toInt(22);
    if (outbound.value("type").toString() != "ssh" || outbound.contains("detour") ||
        !tailscaleAddress(outbound.value("server").toString()) || port < 1 || port > 65535 ||
        outbound.value("user").toString().trimmed().isEmpty() ||
        !QFileInfo(outbound.value("private_key_path").toString()).isAbsolute() ||
        !outbound.value("password").toString().isEmpty() ||
        !outbound.value("private_key").toString().isEmpty() ||
        !outbound.value("private_key_passphrase").toString().isEmpty() ||
        keys.isEmpty() || algorithms.size() != 1 || algorithms[0].toString() != "ssh-ed25519") return false;
    for (const auto& key : keys)
        if (!key.isString() || !key.toString().startsWith("ssh-ed25519 ")) return false;
    return true;
}
inline bool numericId(const QJsonValue& v, int id) {
    return v.isDouble() && v.toDouble() == double(id);
}

inline bool safeRules(const QJsonArray& rules, int id) {
    for (const auto& value : rules) {
        if (!value.isObject()) return false;
        const auto rule = value.toObject();
        const auto action = rule.value("action").toString("route");
        if (action == "route") {
            if (!numericId(rule.value("outbound"), id)) return false;
        } else if (action != "reject" || rule.contains("outbound")) {
            return false;
        }
        // Logical subrules are match conditions only: forbid nested outbound or actions.
        if (rule.contains("rules")) return false;
        if (rule.contains("rule_set")) return false;
    }
    return true;
}

inline QString prepare(QJsonObject& route, QJsonObject& core,
                       const std::map<int, QString>& outbounds, bool verbatim,
                       bool hasAuxiliaryEndpoints) {
    constexpr auto marker = "_mintvpn_strict_amsterdam_outbound";
    constexpr auto selective = "_mintvpn_amsterdam_process_outbound";
    if (route.contains(selective)) {
#ifndef MINTVPN_EXPERIMENTAL_PROCESS_ROUTE
        return "Experimental Amsterdam process route is disabled";
#else
        if (route.contains(marker) || !verbatim || hasAuxiliaryEndpoints)
            return "Amsterdam process route requires an unmodified raw route without auxiliary endpoints";
        const auto value = route.value(selective);
        const int id = value.toInt(-1);
        const auto found = outbounds.find(id);
        if (id < 0 || !numericId(value, id) || found == outbounds.end() || found->second.isEmpty())
            return "Amsterdam process route: selected outbound is unavailable";
        bool pinned = false;
        for (const auto& item : core.value("outbounds").toArray()) {
            const auto outbound = item.toObject();
            if (outbound.value("tag").toString() == found->second) {
                pinned = pinnedSsh(outbound);
                break;
            }
        }
        if (!pinned) return "Amsterdam process route requires pinned Tailscale SSH";
        const auto rules = route.value("rules").toArray();
        if (rules.isEmpty() || !rules.first().isObject())
            return "Amsterdam process route requires the process rule first";
        const auto first = rules.first().toObject();
        const auto processes = first.value("process_name").toArray();
        if (first.size() != 2 || !numericId(first.value("outbound"), id) || processes.isEmpty())
            return "Amsterdam process route requires an exclusive process_name rule first";
        for (const auto& process : processes) {
            const auto name = process.toString();
            if (!process.isString() || !name.endsWith(".exe", Qt::CaseInsensitive) ||
                name.contains('/') || name.contains('\\') || name.contains('*') || name.contains('?'))
                return "Amsterdam process route requires executable names";
        }
        route.remove(selective);
        return {};
#endif
    }
    if (!route.contains(marker)) return {};
    const auto idValue = route.value(marker);
    const int id = idValue.toInt(-1);
    const auto found = outbounds.find(id);
    if (id < 0 || !numericId(idValue, id) || found == outbounds.end() || found->second.isEmpty())
        return "Strict Amsterdam: selected outbound is unavailable";
    if (!verbatim || hasAuxiliaryEndpoints)
        return "Strict Amsterdam requires an unmodified raw route without auxiliary endpoints";
    for (auto it = route.begin(); it != route.end(); ++it) {
        if (it.key() != marker && it.key() != "rules" && it.key() != "final")
            return "Strict Amsterdam: unsupported route option";
    }
    if (!numericId(route.value("final"), id) || !route.value("rules").isArray() ||
        !safeRules(route.value("rules").toArray(), id))
        return "Strict Amsterdam: route or final may bypass the selected outbound";
    bool safeOutbound = false;
    for (const auto& value : core.value("outbounds").toArray()) {
        const auto outbound = value.toObject();
        if (outbound.value("tag").toString() != found->second) continue;
        safeOutbound = pinnedSsh(outbound);
        break;
    }
    if (!safeOutbound) return "Strict Amsterdam requires pinned Tailscale SSH";
    route.remove(marker);
    route["default_domain_resolver"] = "mintvpn-ams-dns";
    core["dns"] = QJsonObject{
        {"servers", QJsonArray{QJsonObject{{"type", "tcp"}, {"tag", "mintvpn-ams-dns"},
                                         {"server", "1.1.1.1"}, {"server_port", 53},
                                         {"detour", found->second}}}},
        {"final", "mintvpn-ams-dns"}
    };
    return {};
}
}
