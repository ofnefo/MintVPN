#pragma once

#include "StrictAmsterdamRoute.h"
#include <QSet>

// Derive a standalone, local HTTP proxy from the real strict SSH configuration.
// This does not enforce that a client uses the proxy: that requires OS isolation.
namespace Configs::ControlledClaudeProxy {
inline QString build(const QJsonObject& generated, int port, QJsonObject& result) {
    if (port < 1024 || port > 65535) return "Invalid local proxy port";
    const auto route = generated.value("route").toObject();
    const auto tag = route.value("final").toString();
    if (tag.isEmpty() || !route.value("rules").isArray() || !route.value("rules").toArray().isEmpty() ||
        route.value("default_domain_resolver").toString() != "mintvpn-ams-dns")
        return "A strict generated Amsterdam SSH route is required";
    QJsonObject ssh;
    int matches = 0;
    for (const auto& value : generated.value("outbounds").toArray()) {
        const auto candidate = value.toObject();
        if (candidate.value("tag").toString() == tag) { ssh = candidate; ++matches; }
    }
    if (matches != 1 || !StrictAmsterdamRoute::pinnedSsh(ssh))
        return "A unique pinned Tailscale SSH outbound is required";
    // Reject unsupported dial options rather than silently changing their meaning
    // or producing a config that the launcher cannot consume.
    const QSet<QString> supported{"type", "tag", "server", "server_port", "user",
        "private_key_path", "host_key", "host_key_algorithms", "client_version"};
    for (auto it = ssh.constBegin(); it != ssh.constEnd(); ++it) {
        if (!supported.contains(it.key()))
            return "Unsupported SSH outbound option for the controlled proxy";
    }
    QJsonObject config{
        {"inbounds", QJsonArray{QJsonObject{{"type", "http"}, {"tag", "claude-http"},
            {"listen", "127.0.0.1"}, {"listen_port", port}}}},
        {"outbounds", QJsonArray{ssh}},
        {"route", QJsonObject{{"final", tag}, {"default_domain_resolver", "mintvpn-ams-dns"}}},
        {"dns", QJsonObject{{"servers", QJsonArray{QJsonObject{{"type", "tcp"},
            {"tag", "mintvpn-ams-dns"}, {"server", "1.1.1.1"}, {"server_port", 53}, {"detour", tag}}}},
            {"final", "mintvpn-ams-dns"}}}
    };
    result = config;
    return {};
}
}
