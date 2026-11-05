#pragma once

#include <QString>

// Only names reserved by the built-in routing manager are presented differently.
// Keep their stored values intact: the routing manager uses them as identifiers.
inline QString managedRouteDisplayName(const QString &name) {
    if (name == QStringLiteral("Quattro • Russia Bypass") ||
        name == QStringLiteral("Quattro Channels") ||
        name == QStringLiteral("Quattro Channels · Bypass Russia"))
        return QStringLiteral("MintVPN") + name.mid(7);
    return name;
}

inline QString managedRuleDisplayName(const QString &name) {
    if (name == QStringLiteral("Quattro Built-in • Russia direct") ||
        name == QStringLiteral("Quattro • Steam domains") ||
        name == QStringLiteral("Quattro • Direct applications") ||
        name == QStringLiteral("Quattro • Google geo channel") ||
        name.startsWith(QStringLiteral("Quattro Built-in • Service • ")) ||
        name.startsWith(QStringLiteral("Quattro • Direct • ")))
        return QStringLiteral("MintVPN") + name.mid(7);
    return name;
}
