#include "include/ui/mainwindow.h"

#include "include/configs/sub/GroupUpdater.hpp"
#include "include/database/GroupsRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/database/RoutesRepo.h"
#include "include/database/SettingsRepo.h"
#include "include/database/entities/RouteRule.h"
#include "include/sys/AutoRun.hpp"
#include "include/stats/autoselector/AutoSelectorMonitor.hpp"
#include "include/ui/widget/QuattroDashboard.hpp"

#include <algorithm>
#include <QIcon>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLibrary>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QTableWidget>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

namespace {
const QString kQuattroRussiaProfileName = QStringLiteral("Quattro • Russia Bypass");
const QString kQuattroRussiaRuleName = QStringLiteral("Quattro Built-in • Russia direct");
const QString kQuattroDirectRulePrefix = QStringLiteral("Quattro • Direct • ");

bool isQuattroSubscriptionDecoration(const QString &name) {
    return name.contains(QStringLiteral("⬇")) || name.contains(QStringLiteral("⚠"))
           || name.trimmed().endsWith(QStringLiteral("---"));
}

bool isQuattroLteServer(const QString &name) {
    static const QRegularExpression lte(QStringLiteral(R"((?i)(?:^|\s|[-|])LTE(?:\s|$|[-|]))"));
    return lte.match(name).hasMatch();
}

QStringList splitRoutingValues(QString value) {
    value.replace(';', ',');
    value.replace('\n', ',');
    QStringList result;
    for (auto item : value.split(',', Qt::SkipEmptyParts)) {
        item = item.trimmed();
        if (!item.isEmpty() && !result.contains(item, Qt::CaseInsensitive)) result << item;
    }
    return result;
}

struct QuattroDirectService {
    QString name;
    QStringList domains;
    QStringList processes;
    QStringList cidrs;
};

QList<QuattroDirectService> defaultQuattroDirectServices() {
    return {
        {QStringLiteral("Яндекс"),
         {QStringLiteral("yandex.ru"), QStringLiteral("yandex.com"), QStringLiteral("yandex.net"),
          QStringLiteral("ya.ru"), QStringLiteral("yastatic.net"), QStringLiteral("yadi.sk"),
          QStringLiteral("yandexcloud.ru"), QStringLiteral("yandexcloud.net")},
         {QStringLiteral("YandexDisk.exe"), QStringLiteral("YandexDisk2.exe")}},
        {QStringLiteral("VK / Mail.ru"),
         {QStringLiteral("vk.com"), QStringLiteral("vk.ru"), QStringLiteral("userapi.com"),
          QStringLiteral("mail.ru"), QStringLiteral("mycdn.me")}, {}},
        {QStringLiteral("Ozon / Wildberries"),
         {QStringLiteral("ozon.ru"), QStringLiteral("ozonusercontent.com"),
          QStringLiteral("wildberries.ru"), QStringLiteral("wb.ru"),
          QStringLiteral("wbbasket.ru")}, {}},
        {QStringLiteral("Банки / платежи / Госуслуги"),
         {QStringLiteral("sberbank.ru"), QStringLiteral("sber.ru"), QStringLiteral("sberbank.com"),
          QStringLiteral("tbank.ru"), QStringLiteral("tinkoff.ru"), QStringLiteral("tinkoffbank.ru"),
          QStringLiteral("vtb.ru"), QStringLiteral("alfabank.ru"), QStringLiteral("alfabank.com"),
          QStringLiteral("gazprombank.ru"), QStringLiteral("raiffeisen.ru"), QStringLiteral("rshb.ru"),
          QStringLiteral("psbank.ru"), QStringLiteral("sovcombank.ru"), QStringLiteral("rosbank.ru"),
          QStringLiteral("nspk.ru"), QStringLiteral("sbp.nspk.ru"), QStringLiteral("mironline.ru"),
          QStringLiteral("yoomoney.ru"), QStringLiteral("yookassa.ru"), QStringLiteral("gosuslugi.ru"),
          QStringLiteral("nalog.gov.ru")}, {}},
        {QStringLiteral("Windows Update"),
         {QStringLiteral("windowsupdate.com"), QStringLiteral("windowsupdate.microsoft.com"),
          QStringLiteral("update.microsoft.com"), QStringLiteral("download.microsoft.com"),
          QStringLiteral("delivery.mp.microsoft.com"), QStringLiteral("do.dsp.mp.microsoft.com")}, {}},
        {QStringLiteral("Tailscale"),
         {QStringLiteral("tailscale.com"), QStringLiteral("tailscale.io"), QStringLiteral("ts.net")},
         {QStringLiteral("tailscaled.exe")}},
        {QStringLiteral("2ГИС"),
         {QStringLiteral("2gis.ru"), QStringLiteral("2gis.com"), QStringLiteral("dgis.com")}, {}},
    };
}

// Developer and AI endpoints that are reachable from Russia and measured faster
// direct than through the tunnel (model/package downloads, API calls). Services
// that geo-block Russia (OpenAI, Anthropic, Atlas Cloud, fal.ai) are left on the
// VPN path. Measurements: NETWORK_AUDIT_2026-09-25.md.
QList<QuattroDirectService> defaultQuattroWorkDirectServices() {
    return {
        {QStringLiteral("Hugging Face"),
         {QStringLiteral("huggingface.co"), QStringLiteral("hf.co")}, {}},
        {QStringLiteral("Python: PyPI / PyTorch"),
         {QStringLiteral("pypi.org"), QStringLiteral("pythonhosted.org"),
          QStringLiteral("pytorch.org")}, {}},
        {QStringLiteral("GitHub"),
         {QStringLiteral("github.com"), QStringLiteral("githubusercontent.com")}, {}},
        {QStringLiteral("npm"),
         {QStringLiteral("registry.npmjs.org")}, {}},
        // Model files are served from Civitai's own R2 bucket, not all of R2.
        {QStringLiteral("Civitai"),
         {QStringLiteral("civitai.com"),
          QStringLiteral("5ac0637cfd0766c97916cefa3764fbdf.r2.cloudflarestorage.com")}, {}},
        {QStringLiteral("AI API: BytePlus / Kie.ai"),
         {QStringLiteral("bytepluses.com"), QStringLiteral("kie.ai")}, {}},
        // The owner's requested direct routes; keep these domain-scoped so the
        // CLI executable's unrelated connections are not silently redirected.
        {QStringLiteral("GLM / Z.ai"),
         {QStringLiteral("z.ai"), QStringLiteral("bigmodel.cn")}, {}},
        {QStringLiteral("Kimi / Moonshot"),
         {QStringLiteral("kimi.com"), QStringLiteral("moonshot.ai"), QStringLiteral("moonshot.cn")}, {}},
        {QStringLiteral("MiniMax"),
         {QStringLiteral("minimax.io"), QStringLiteral("minimax.chat"), QStringLiteral("minimaxi.com")}, {}},
        // Search, Drive, Docs and Google APIs. Gemini and YouTube are pinned to
        // the VPN by defaultQuattroVpnPinnedServices(), which is matched first.
        {QStringLiteral("Google / Drive"),
         {QStringLiteral("google.com"), QStringLiteral("google.ru"),
          QStringLiteral("googleapis.com"), QStringLiteral("gstatic.com"),
          QStringLiteral("googleusercontent.com"), QStringLiteral("googledrive.com"),
          QStringLiteral("gvt1.com"), QStringLiteral("gvt2.com")}, {}},
        // Remote desktop: Tailscale peer traffic and Sunshine/Moonlight streams must
        // never be wrapped in the VPN tunnel (latency, bitrate headroom).
        {QStringLiteral("Tailscale / Moonlight"),
         {QStringLiteral("tailscale.com"), QStringLiteral("tailscale.io")},
         {QStringLiteral("tailscaled.exe"), QStringLiteral("tailscale-ipn.exe"),
          QStringLiteral("sunshine.exe"), QStringLiteral("Moonlight.exe")},
         {QStringLiteral("100.64.0.0/10")}},
        // Direct rules for locally configured SSH and file-transfer clients.
        {QStringLiteral("SSH / SFTP"), {},
         {QStringLiteral("ssh.exe"), QStringLiteral("scp.exe"), QStringLiteral("sftp.exe"),
          QStringLiteral("sshd.exe"), QStringLiteral("plink.exe"), QStringLiteral("WinSCP.exe"),
          QStringLiteral("sshfs.exe"), QStringLiteral("rclone.exe")}, {}},
    };
}

// Services that must always use the VPN even when a broader direct rule (Google,
// geosite-ru) would match. Rules are first-match, so these are placed first.
QList<QuattroDirectService> defaultQuattroVpnPinnedServices() {
    return {
        {QStringLiteral("Anthropic / Claude"),
         {QStringLiteral("anthropic.com"), QStringLiteral("claude.ai"),
          QStringLiteral("claude.com"), QStringLiteral("claudeusercontent.com")}, {}},
        {QStringLiteral("OpenAI / ChatGPT / Codex"),
         {QStringLiteral("openai.com"), QStringLiteral("chatgpt.com"),
          QStringLiteral("oaistatic.com"), QStringLiteral("oaiusercontent.com")}, {}},
        {QStringLiteral("Cursor"),
         {QStringLiteral("cursor.com"), QStringLiteral("cursor.sh"), QStringLiteral("anysphere.com")}, {}},
        {QStringLiteral("Atlas Cloud / fal.ai"),
         {QStringLiteral("atlascloud.ai"), QStringLiteral("fal.ai"), QStringLiteral("fal.media")}, {}},
        {QStringLiteral("Gemini / AI Studio"),
         {QStringLiteral("gemini.google.com"), QStringLiteral("aistudio.google.com"),
          QStringLiteral("generativelanguage.googleapis.com"),
          QStringLiteral("notebooklm.google.com"), QStringLiteral("notebooklm.google")}, {}},
        {QStringLiteral("YouTube"),
         {QStringLiteral("youtube.com"), QStringLiteral("youtu.be"), QStringLiteral("ytimg.com"),
          QStringLiteral("googlevideo.com"), QStringLiteral("ggpht.com"),
          QStringLiteral("youtube-nocookie.com")}, {}},
    };
}

QStringList quattroGoogleChannelDomains() {
    return {QStringLiteral("gemini.google.com"), QStringLiteral("aistudio.google.com"),
            QStringLiteral("generativelanguage.googleapis.com"),
            QStringLiteral("notebooklm.google.com"), QStringLiteral("youtube.com"),
            QStringLiteral("youtu.be"), QStringLiteral("ytimg.com"),
            QStringLiteral("googlevideo.com"), QStringLiteral("ggpht.com")};
}

bool isRetiredQuattroDirectService(const QString &name) {
    return name.compare(QStringLiteral("AnyDesk"), Qt::CaseInsensitive) == 0;
}

void removeRetiredQuattroDirectServices() {
    for (const auto &profile : Configs::dataManager->routesRepo->GetAllRouteProfiles()) {
        if (!profile || profile->isRaw) continue;
        QList<std::shared_ptr<Configs::RouteRule>> kept;
        kept.reserve(profile->Rules.size());
        bool changed = false;
        for (const auto &rule : profile->Rules) {
            const bool managedDirectRule = rule && rule->name.startsWith(kQuattroDirectRulePrefix);
            const auto serviceName = managedDirectRule
                                         ? rule->name.mid(kQuattroDirectRulePrefix.size())
                                         : QString();
            if (managedDirectRule && isRetiredQuattroDirectService(serviceName)) {
                changed = true;
                continue;
            }
            kept << rule;
        }
        if (changed) {
            profile->Rules = kept;
            Configs::dataManager->routesRepo->Save(profile);
        }
    }
}

int quattroSubscriptionDisplayRank(const QString &name) {
    const bool fastestAuto = name.contains(QStringLiteral("Авто"), Qt::CaseInsensitive)
                             && name.contains(QStringLiteral("Самый быстрый"), Qt::CaseInsensitive);
    if (!fastestAuto) return 2;
    return name.contains(QStringLiteral("💎")) ? 0 : 1;
}

std::shared_ptr<Configs::RouteProfile> ensureQuattroRussiaProfile() {
    auto &repo = Configs::dataManager->routesRepo;
    std::shared_ptr<Configs::RouteProfile> profile;
    for (const auto &candidate : repo->GetAllRouteProfiles()) {
        if (candidate && candidate->name == kQuattroRussiaProfileName) {
            profile = candidate;
            break;
        }
    }

    if (!profile) {
        profile = Configs::RouteProfile::GetDefaultChain();
        profile->name = kQuattroRussiaProfileName;
    }

    std::shared_ptr<Configs::RouteRule> managedRule;
    for (const auto &rule : profile->Rules) {
        if (rule && rule->name == kQuattroRussiaRuleName) {
            managedRule = rule;
            break;
        }
    }
    if (!managedRule) {
        managedRule = std::make_shared<Configs::RouteRule>();
        managedRule->name = kQuattroRussiaRuleName;
        profile->Rules << managedRule;
    }
    managedRule->type = Configs::simpleAddressBypass;
    managedRule->action = QStringLiteral("route");
    managedRule->outboundID = Configs::directID;
    managedRule->rule_set = {QStringLiteral("geosite-ru"), QStringLiteral("geoip-ru")};

    // Domain rules are separate from geosite/geoip: conditions on a single rule
    // would be ANDed. Put these before geographic IP matching.
    const QString builtinPrefix = QStringLiteral("Quattro Built-in • Service • ");
    QList<std::shared_ptr<Configs::RouteRule>> services;
    for (const auto &service : defaultQuattroVpnPinnedServices()) {
        auto rule = std::make_shared<Configs::RouteRule>();
        rule->name = builtinPrefix + QStringLiteral("VPN • ") + service.name;
        rule->type = Configs::simpleAddressProxy;
        rule->action = QStringLiteral("route");
        rule->outboundID = Configs::proxyID;
        for (const auto &existing : profile->Rules) {
            if (existing && existing->name == rule->name && existing->action == QStringLiteral("route")
                && existing->outboundID > 0
                && Configs::dataManager->profilesRepo->GetProfile(existing->outboundID)) {
                rule->outboundID = existing->outboundID;
                rule->type = Configs::custom;
                break;
            }
        }
        rule->domain_suffix = service.domains;
        services << rule;
    }
    for (const auto &service : defaultQuattroDirectServices() + defaultQuattroWorkDirectServices()) {
        auto rule = std::make_shared<Configs::RouteRule>();
        rule->name = builtinPrefix + service.name;
        rule->type = Configs::simpleAddressBypass;
        rule->action = QStringLiteral("route");
        rule->outboundID = Configs::directID;
        rule->domain_suffix = service.domains;
        services << rule;
        if (!service.processes.isEmpty()) {
            auto process = std::make_shared<Configs::RouteRule>();
            process->name = builtinPrefix + service.name + QStringLiteral(" applications");
            process->type = Configs::simpleProcessNameBypass;
            process->action = QStringLiteral("route");
            process->outboundID = Configs::directID;
            process->process_name = service.processes;
            services << process;
        }
        if (!service.cidrs.isEmpty()) {
            // Separate rule: address conditions on one rule would be ANDed with domains.
            auto cidr = std::make_shared<Configs::RouteRule>();
            cidr->name = builtinPrefix + service.name + QStringLiteral(" addresses");
            cidr->type = Configs::simpleAddressBypass;
            cidr->action = QStringLiteral("route");
            cidr->outboundID = Configs::directID;
            cidr->ip_cidr = service.cidrs;
            services << cidr;
        }
    }
    auto local = std::make_shared<Configs::RouteRule>();
    local->name = builtinPrefix + QStringLiteral("Local networks");
    local->type = Configs::simpleAddressBypass;
    local->action = QStringLiteral("route");
    local->outboundID = Configs::directID;
    local->ip_cidr = {QStringLiteral("10.0.0.0/8"), QStringLiteral("100.64.0.0/10"),
                      QStringLiteral("127.0.0.0/8"), QStringLiteral("169.254.0.0/16"),
                      QStringLiteral("172.16.0.0/12"), QStringLiteral("192.168.0.0/16"),
                      QStringLiteral("::1/128"), QStringLiteral("fc00::/7"), QStringLiteral("fe80::/10")};
    services << local;
    QList<std::shared_ptr<Configs::RouteRule>> processOverrides, retained;
    for (const auto &rule : profile->Rules) {
        if (!rule || rule->name.startsWith(builtinPrefix)) continue;
        const bool hasProcess = !rule->process_name.isEmpty() || !rule->process_path.isEmpty()
            || !rule->process_path_regex.isEmpty();
        if (hasProcess && rule->action == QStringLiteral("route") && rule->outboundID > 0
            && Configs::dataManager->profilesRepo->GetProfile(rule->outboundID))
            processOverrides << rule;
        else
            retained << rule;
    }
    profile->Rules = processOverrides + services + retained;

    profile->isRaw = false;
    profile->isRemote = false;
    profile->remoteURL.clear();
    profile->autoUpdate = false;
    profile->preventModifications = false;
    if (profile->id < 0) {
        if (!repo->AddRouteProfile(profile)) return nullptr;
    } else if (!repo->Save(profile)) {
        return nullptr;
    }
    return profile;
}

void ensureQuattroLocalNetworkBypass() {
    auto &settings = Configs::dataManager->settingsRepo;
    const QStringList requiredRanges = {
        QStringLiteral("10.0.0.0/8"),
        QStringLiteral("100.64.0.0/10"),
        QStringLiteral("169.254.0.0/16"),
        QStringLiteral("172.16.0.0/12"),
        QStringLiteral("192.168.0.0/16"),
        QStringLiteral("224.0.0.0/4"),
        QStringLiteral("::1/128"),
        QStringLiteral("fc00::/7"),
        QStringLiteral("fe80::/10"),
        QStringLiteral("ff00::/8"),
    };
    bool changed = false;
    if (settings->disable_private_range_bypass) {
        settings->disable_private_range_bypass = false;
        changed = true;
    }
    for (const auto &range : requiredRanges) {
        if (!settings->vpn_private_ranges.contains(range)) {
            settings->vpn_private_ranges << range;
            changed = true;
        }
    }
    if (changed) settings->Save();
}
}

void MainWindow::setupQuattroDashboard() {
    if (quattroDashboard != nullptr) return;

    ensureQuattroLocalNetworkBypass();
    removeRetiredQuattroDirectServices();
    const auto russiaProfile = ensureQuattroRussiaProfile();
    // Refresh only shipped rules in existing channel profiles; keep user rules first.
    if (russiaProfile) {
        for (const auto &route : Configs::dataManager->routesRepo->GetAllRouteProfiles()) {
            if (!route || route->isRaw || !route->name.startsWith(QStringLiteral("Quattro Channels"))
                || !route->name.contains(QStringLiteral("Russia"))) continue;
            QList<std::shared_ptr<Configs::RouteRule>> rules;
            for (const auto &rule : route->Rules) {
                if (!rule || rule->name.startsWith(QStringLiteral("Quattro Built-in •"))) continue;
                // Older builds sent all of Google through this channel, overriding Drive direct.
                if (rule->name == QStringLiteral("Quattro • Google geo channel"))
                    rule->domain_suffix = quattroGoogleChannelDomains();
                rules << rule;
            }
            for (const auto &rule : russiaProfile->Rules)
                if (rule && rule->name.startsWith(QStringLiteral("Quattro Built-in •")))
                    rules << std::make_shared<Configs::RouteRule>(*rule);
            route->Rules = rules;
            Configs::dataManager->routesRepo->Save(route);
        }
    }
    const auto currentRoute = Configs::dataManager->routesRepo->GetRouteProfile(
        Configs::dataManager->settingsRepo->current_route_id);
    // Product default: local and Russian traffic is direct. Preserve explicit
    // custom routing, but migrate the untouched upstream Default profile.
    if (russiaProfile && currentRoute
        && currentRoute->name.compare(QStringLiteral("Default"), Qt::CaseInsensitive) == 0
        && !currentRoute->isRaw && currentRoute->Rules.size() == 1
        && currentRoute->Rules.first()->action == QStringLiteral("hijack-dns")) {
        Configs::dataManager->settingsRepo->current_route_id = russiaProfile->id;
        Configs::dataManager->settingsRepo->Save();
    }
    ensureQuattroAutoSelector();

    auto *legacyCentralWidget = takeCentralWidget();
    quattroStack = new QStackedWidget(this);
    advancedCentralWidget = new QWidget(quattroStack);
    advancedCentralWidget->setObjectName(QStringLiteral("quattroAdvanced"));
    auto *advancedLayout = new QVBoxLayout(advancedCentralWidget);
    advancedLayout->setContentsMargins(0, 0, 0, 0);
    advancedLayout->setSpacing(0);

    auto *advancedHeader = new QFrame(advancedCentralWidget);
    advancedHeader->setObjectName(QStringLiteral("advancedHeader"));
    auto *advancedHeaderLayout = new QHBoxLayout(advancedHeader);
    advancedHeaderLayout->setContentsMargins(16, 11, 16, 11);
    advancedHeaderLayout->setSpacing(11);
    auto *advancedLogo = new QLabel(advancedHeader);
    advancedLogo->setPixmap(QPixmap(QStringLiteral(":/Quattro/Quattro.png")).scaled(
        32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    advancedLogo->setFixedSize(34, 34);
    advancedHeaderLayout->addWidget(advancedLogo);
    auto *advancedBrand = new QVBoxLayout;
    advancedBrand->setSpacing(0);
    auto *advancedTitle = new QLabel(tr("MintVPN Workspace"), advancedHeader);
    advancedTitle->setObjectName(QStringLiteral("advancedTitle"));
    auto *advancedHint = new QLabel(tr("Расширенное управление профилями и маршрутами"), advancedHeader);
    advancedHint->setObjectName(QStringLiteral("advancedHint"));
    advancedBrand->addWidget(advancedTitle);
    advancedBrand->addWidget(advancedHint);
    advancedHeaderLayout->addLayout(advancedBrand);
    advancedHeaderLayout->addSpacing(12);
    ui->data_view->setMinimumWidth(0);
    ui->data_view->setMaximumHeight(42);
    advancedHeaderLayout->addWidget(ui->data_view, 1);
    auto *advancedBadge = new QLabel(tr("ADVANCED"), advancedHeader);
    advancedBadge->setObjectName(QStringLiteral("advancedBadge"));
    advancedHeaderLayout->addWidget(advancedBadge);
    auto *backButton = new QPushButton(tr("← Главная"), advancedHeader);
    backButton->setObjectName(QStringLiteral("advancedBack"));
    backButton->setCursor(Qt::PointingHandCursor);
    backButton->setMinimumHeight(38);
    advancedHeaderLayout->addWidget(backButton);
    advancedLayout->addWidget(advancedHeader);
    advancedLayout->addWidget(legacyCentralWidget, 1);

    if (auto *layout = legacyCentralWidget->layout()) {
        layout->setContentsMargins(14, 12, 14, 14);
        layout->setSpacing(10);
    }

    const QList<QToolButton *> advancedNav = {
        ui->toolButton_program, ui->toolButton_preferences, ui->toolButton_testing,
        ui->toolButton_routing, ui->toolButton_tools,
    };
    const QStringList advancedLabels = {
        tr("Меню"), tr("Параметры"), tr("Проверка"), tr("Маршруты"), tr("Инструменты"),
    };
    for (int i = 0; i < advancedNav.size(); ++i) {
        auto *button = advancedNav.at(i);
        button->setText(advancedLabels.at(i));
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setIconSize(QSize(18, 18));
        button->setMinimumWidth(0);
        button->setMaximumWidth(132);
        button->setMinimumHeight(42);
        button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        button->setStyleSheet(QString());
    }
    ui->tabWidget->setStyleSheet(QString());
    ui->profilesTableView->setStyleSheet(QString());

    advancedCentralWidget->setStyleSheet(QStringLiteral(R"qss(
        QWidget#quattroAdvanced, QWidget#quattroAdvanced > QWidget {
            background: #141a23;
            color: #edf1f6;
            font-family: "Segoe UI Variable Text", "Segoe UI";
            font-size: 13px;
        }
        QFrame#advancedHeader {
            background: #171e27;
            border-bottom: 1px solid #303946;
        }
        QLabel#advancedTitle { color: #ffffff; font-size: 16px; font-weight: 750; }
        QLabel#advancedHint { color: #929dab; font-size: 11px; }
        QLabel#advancedBadge {
            color: #65ddeb;
            background: #173c48;
            border: 1px solid #2bbfd4;
            border-radius: 10px;
            padding: 4px 8px;
            font-size: 10px;
            font-weight: 750;
        }
        QPushButton#advancedBack {
            color: #ecf1f6;
            background: #222a35;
            border: 1px solid #3a4553;
            border-radius: 9px;
            padding: 0 13px;
            font-weight: 650;
        }
        QPushButton#advancedBack:hover { border-color: #48d0e1; color: #6de2ef; }
        QToolButton {
            color: #dfe5ec;
            background: #1b222c;
            border: 1px solid #303946;
            border-radius: 9px;
            padding: 7px 9px;
            font-weight: 600;
        }
        QToolButton:hover, QToolButton:checked {
            color: #6de2ef;
            background: #1c333e;
            border-color: #37c7d9;
        }
        QToolButton::menu-indicator { image: none; width: 0; }
        QCheckBox { color: #d9dfe7; spacing: 7px; }
        QSplitter::handle { background: #303946; height: 2px; }
        QTabWidget::pane {
            background: #1a212b;
            border: 1px solid #303946;
            border-radius: 10px;
            top: -1px;
        }
        QTabBar { background: transparent; qproperty-drawBase: 0; }
        QTabBar::tab {
            color: #9fa9b6;
            background: #181f28;
            border: 1px solid #303946;
            border-bottom: none;
            border-top-left-radius: 8px;
            border-top-right-radius: 8px;
            padding: 8px 14px;
            margin-right: 4px;
        }
        QTabBar::tab:selected { color: #69e0ed; background: #1c333e; border-color: #36c5d8; }
        QTableView, QTreeView, QListView, QTextEdit, QPlainTextEdit, QTextBrowser {
            color: #e8edf3;
            background: #171e27;
            alternate-background-color: #1b232d;
            border: 1px solid #303946;
            border-radius: 8px;
            gridline-color: #2b3440;
            selection-background-color: #174b58;
            selection-color: #ffffff;
        }
        QHeaderView::section {
            color: #aeb7c3;
            background: #202833;
            border: none;
            border-right: 1px solid #303946;
            border-bottom: 1px solid #303946;
            padding: 7px 8px;
            font-weight: 650;
        }
        QLineEdit, QComboBox, QSpinBox {
            color: #edf1f6;
            background: #151b24;
            border: 1px solid #3a4553;
            border-radius: 7px;
            padding: 5px 8px;
            selection-background-color: #1f8291;
        }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus { border-color: #3bd0e3; }
        QPushButton {
            color: #e4e9ef;
            background: #222a35;
            border: 1px solid #3a4553;
            border-radius: 8px;
            padding: 6px 11px;
        }
        QPushButton:hover { color: #6de2ef; border-color: #43cddd; background: #1c333e; }
        QScrollBar:vertical { background: #141a23; width: 8px; }
        QScrollBar::handle:vertical { background: #3d4856; border-radius: 4px; min-height: 26px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
    )qss"));

    const QString menuStyle = QStringLiteral(
        "QMenu{background:#1b222c;color:#edf1f6;border:1px solid #3a4553;padding:6px;}"
        "QMenu::item{padding:7px 28px 7px 10px;border-radius:6px;}"
        "QMenu::item:selected{background:#174b58;color:#ffffff;}"
        "QMenu::separator{height:1px;background:#303946;margin:5px 8px;}");
    for (auto *menu : {ui->menu_program, ui->menu_preferences, ui->menuTesting,
                       ui->menuRouting_Menu, ui->menuTools}) {
        menu->setStyleSheet(menuStyle);
    }

    quattroDashboard = new QuattroDashboard(quattroStack);
    quattroStack->addWidget(quattroDashboard);
    quattroStack->addWidget(advancedCentralWidget);
    quattroStack->setCurrentWidget(quattroDashboard);
    setCentralWidget(quattroStack);

    setWindowIcon(QIcon(QStringLiteral(":/Quattro/Quattro.png")));
    setWindowTitle(QStringLiteral("MintVPN"));
    resize(qMax(width(), 940), qMax(height(), 760));

    connect(backButton, &QPushButton::clicked, this, [this] {
        quattroStack->setCurrentWidget(quattroDashboard);
    });

    connect(quattroDashboard, &QuattroDashboard::advancedRequested, this, [this] {
        quattroStack->setCurrentWidget(advancedCentralWidget);
    });
    connect(quattroDashboard, &QuattroDashboard::settingsRequested,
            this, &MainWindow::on_menu_basic_settings_triggered);
    connect(quattroDashboard, &QuattroDashboard::routingRequested,
            this, &MainWindow::on_menu_routing_settings_triggered);
    connect(quattroDashboard, &QuattroDashboard::channelsRequested,
            this, &MainWindow::showQuattroChannels);
    connect(quattroDashboard, &QuattroDashboard::recoverRequested, this, [this] {
#ifdef Q_OS_WIN
        QLibrary dnsLibrary(QStringLiteral("dnsapi"));
        using FlushDns = int (__stdcall *)();
        const auto flush = reinterpret_cast<FlushDns>(dnsLibrary.resolve("DnsFlushResolverCache"));
        const bool cleared = flush && flush();
        MW_show_log(cleared ? tr("DNS-кэш Windows очищен.") : tr("Не удалось очистить DNS-кэш Windows."));
#endif
        const int id = Configs::dataManager->settingsRepo->started_id;
        if (id >= 0) profile_start(id);
        else MW_show_log(tr("VPN отключён; восстановление не запускает новое подключение."));
    });
    connect(quattroDashboard, &QuattroDashboard::subscriptionSubmitted,
            this, &MainWindow::importQuattroSubscription);
    connect(quattroDashboard, &QuattroDashboard::subscriptionRefreshRequested, this, [this] {
        const auto group = Configs::dataManager->groupsRepo->CurrentGroup();
        if (!group || group->url.isEmpty()) {
            MessageBoxWarning(tr("Подписка VPN"), tr("Сначала добавьте ссылку подписки."));
            return;
        }
        quattroDashboard->setSubscriptionState(true, tr("Обновляю список серверов…"));
        Subscription::groupUpdater->AsyncUpdate(group->url, group->id, nullptr, true, [this](bool success) {
            runOnUiThread([this, success] {
                refreshQuattroDashboard(true);
                quattroDashboard->setSubscriptionState(false, success
                    ? tr("Подписка обновлена.")
                    : tr("Обновление не удалось. Подробности в журнале."));
            });
        });
    });
    connect(quattroDashboard, &QuattroDashboard::modeChanged, this,
            [this](QuattroDashboard::Mode mode) {
        quattroSelectedMode = static_cast<int>(mode);
        if (mode == QuattroDashboard::Mode::Tun) {
            set_spmode_system_proxy(false);
            set_spmode_vpn(true);
        } else {
            set_spmode_vpn(false);
            set_spmode_system_proxy(true);
        }
        refreshQuattroDashboard();
    });
    connect(quattroDashboard, &QuattroDashboard::profileChanged, this, [this](int id) {
        if (id < -1) return;
        quattroSelectedProfileId = id;
        const int targetId = id < 0 ? ensureQuattroAutoSelector() : id;
        auto &settings = Configs::dataManager->settingsRepo;
        if (settings->remember_enable) {
            settings->remember_id = targetId;
            settings->Save();
        }
        if (running && targetId >= 0 && running->id != targetId) profile_start(targetId);
    });
    connect(quattroDashboard, &QuattroDashboard::connectionToggled, this, [this](bool connectNow) {
        if (!connectNow) {
            profile_stop(false, false, true);
            return;
        }
        if (quattroSelectedMode == static_cast<int>(QuattroDashboard::Mode::Tun)) {
            set_spmode_system_proxy(false);
            set_spmode_vpn(true);
            if (!Configs::dataManager->settingsRepo->spmode_vpn) return;
        } else {
            set_spmode_vpn(false);
            set_spmode_system_proxy(true);
            if (!Configs::dataManager->settingsRepo->spmode_system_proxy) return;
        }
        const int id = quattroSelectedProfileId < 0 ? ensureQuattroAutoSelector() : quattroSelectedProfileId;
        if (id < 0) {
            MessageBoxWarning(tr("MintVPN"), tr("В подписке пока нет доступных серверов."));
            return;
        }
        profile_start(id);
    });
    connect(quattroDashboard, &QuattroDashboard::russiaBypassChanged,
            this, &MainWindow::setQuattroRussiaBypass);
    connect(quattroDashboard, &QuattroDashboard::autoStartChanged, this, [this](bool enabled) {
        auto &settings = Configs::dataManager->settingsRepo;
        int rememberedId = settings->remember_id;
        if (enabled) {
            rememberedId = quattroSelectedProfileId < 0
                               ? ensureQuattroAutoSelector()
                               : quattroSelectedProfileId;
            if (rememberedId < 0) {
                quattroDashboard->setAutoStart(false);
                MessageBoxWarning(tr("Автоподключение"), tr("Сначала добавьте подписку и дождитесь списка серверов."));
                return;
            }
        }
        settings->remember_enable = enabled;
        if (enabled) {
            settings->remember_id = rememberedId;
            settings->remember_tun = quattroSelectedMode == static_cast<int>(QuattroDashboard::Mode::Tun);
            settings->remember_system_proxy = !settings->remember_tun;
        }
        settings->Save();
        AutoRun_SetEnabled(enabled);
        const bool active = AutoRun_IsEnabled();
        if (enabled != active) {
            settings->remember_enable = active;
            settings->Save();
            MessageBoxWarning(tr("Автоподключение"), tr("Не удалось изменить автозапуск в Windows. Проверьте задачу в планировщике."));
        }
        ui->actionStart_with_system->setChecked(active);
        ui->actionRemember_last_proxy->setChecked(active);
        quattroDashboard->setAutoStart(active);
    });

    refreshQuattroDashboard(true);
}

void MainWindow::refreshQuattroDashboard(bool reloadProfiles) {
    if (quattroDashboard == nullptr) return;

    const auto &settings = Configs::dataManager->settingsRepo;
    QString server = running ? running->outbound->DisplayName() : QString();
    if (running && running->type == "autoselector") {
        const auto selector = Stats::autoSelectorMonitor->Snapshot();
        if (selector.valid && !selector.selectedName.isEmpty()) server = selector.selectedName;
    }
    QString status;
    if (m_profileConnecting) status = tr("Подключение…");
    else if (m_profileDisconnecting) status = tr("Отключение…");
    else status = running ? tr("Подключено") : tr("Отключено");
    quattroDashboard->setConnectionState(running != nullptr, status, server,
                                          m_profileConnecting || m_profileDisconnecting);
    const auto selectedMode = settings->spmode_system_proxy && !settings->spmode_vpn
                                  ? QuattroDashboard::Mode::SystemProxy
                                  : QuattroDashboard::Mode::Tun;
    quattroSelectedMode = static_cast<int>(selectedMode);
    quattroDashboard->setMode(selectedMode);
    quattroDashboard->setAutoStart(settings->remember_enable && ui->actionStart_with_system->isChecked());

    const auto route = Configs::dataManager->routesRepo->GetRouteProfile(settings->current_route_id);
    quattroDashboard->setRussiaBypass(route && route->name.contains("Russia", Qt::CaseInsensitive));

    if (!reloadProfiles) {
        if (running) {
            quattroSelectedProfileId = running->type == "autoselector" ? -1 : running->id;
            quattroDashboard->setSelectedProfile(quattroSelectedProfileId);
        }
        return;
    }
    // Subscription updates may replace/reorder profile IDs. Rebuild the Auto
    // tiers from the current section layout before presenting the fresh list.
    ensureQuattroAutoSelector();
    QList<QPair<int, QString>> items;
    int serverCount = 0;
    const auto group = Configs::dataManager->groupsRepo->CurrentGroup();
    if (group) {
        for (const auto &profile : Configs::dataManager->profilesRepo->GetProfileBatch(group->Profiles())) {
            if (!profile || profile->type == "autoselector"
                || isQuattroSubscriptionDecoration(profile->name)) continue;
            const auto displayName = profile->outbound->DisplayName();
            if (isQuattroSubscriptionDecoration(displayName)) {
                // Preserve the subscription's sections and exact order in the
                // manual picker, but mark decoration rows as non-connectable.
                items << qMakePair(-profile->id - 2, displayName);
            } else {
                items << qMakePair(profile->id, displayName);
                serverCount++;
            }
        }
        // Quattro publishes its working Auto endpoints at the end of the raw
        // subscription. The regular client presents that block first. Mirror
        // that view while preserving every name and the relative order of all
        // other subscription rows.
        std::stable_sort(items.begin(), items.end(), [](const auto &left, const auto &right) {
            return quattroSubscriptionDisplayRank(left.second)
                   < quattroSubscriptionDisplayRank(right.second);
        });
    }
    int selected = running ? (running->type == "autoselector" ? -1 : running->id)
                           : quattroSelectedProfileId;
    if (selected < 0 && settings->remember_id >= 0) {
        const auto remembered = Configs::dataManager->profilesRepo->GetProfile(settings->remember_id);
        if (remembered && remembered->type != "autoselector") selected = remembered->id;
    }
    // A removed profile is absent from the picker. Fall back to Auto instead of
    // connecting to a stale server while the UI appears to have no selection.
    if (selected >= 0) {
        const auto selectedItem = std::find_if(items.cbegin(), items.cend(), [selected](const auto &item) {
            return item.first == selected;
        });
        if (selectedItem == items.cend()) selected = -1;
    }
    quattroSelectedProfileId = selected;
    quattroDashboard->setProfiles(items, selected);
    if (group && !group->url.isEmpty()) {
        quattroDashboard->setSubscriptionState(
            false, items.isEmpty() ? tr("Подписка сохранена, но серверы пока не загружены")
                                   : tr("Подписка активна • %1 серверов").arg(serverCount));
    } else {
        quattroDashboard->setSubscriptionState(false, tr("Вставьте ссылку подписки, чтобы начать"));
    }
}

void MainWindow::importQuattroSubscription(const QString &url) {
    const QUrl parsed(url);
    if (!parsed.isValid() || (parsed.scheme() != "http" && parsed.scheme() != "https") || parsed.host().isEmpty()) {
        MessageBoxWarning(tr("Подписка VPN"), tr("Нужна корректная ссылка http(s) на подписку."));
        return;
    }

    std::shared_ptr<Configs::Group> group;
    for (const int id : Configs::dataManager->groupsRepo->GetAllGroupIds()) {
        const auto candidate = Configs::dataManager->groupsRepo->GetGroup(id);
        if (candidate && QUrl(candidate->url).host().compare(parsed.host(), Qt::CaseInsensitive) == 0) {
            group = candidate;
            break;
        }
    }
    if (!group) {
        const auto current = Configs::dataManager->groupsRepo->CurrentGroup();
        if (current && current->Profiles().isEmpty() && current->url.isEmpty()) group = current;
    }
    if (!group) {
        group = Configs::GroupsRepo::NewGroup();
        group->name = QStringLiteral("MintVPN");
        Configs::dataManager->groupsRepo->AddGroup(group);
    }

    group->url = url;
    group->skip_auto_update = false;
    Configs::dataManager->groupsRepo->Save(group);
    Configs::dataManager->settingsRepo->current_group = group->id;
    Configs::dataManager->settingsRepo->sub_auto_update = 60;
    Configs::dataManager->settingsRepo->Save();
    refresh_groups();
    show_group(group->id);

    quattroDashboard->setSubscriptionState(true, tr("Проверяю подписку и загружаю серверы…"));
    Subscription::groupUpdater->AsyncUpdate(url, group->id, [this] {
        runOnUiThread([this] {
            refreshQuattroDashboard(true);
            const auto current = Configs::dataManager->groupsRepo->CurrentGroup();
            const int count = current ? current->Profiles().size() : 0;
            quattroDashboard->setSubscriptionState(
                false, count > 0 ? tr("Готово: %1 серверов").arg(count)
                                 : tr("Серверы не получены. Проверьте ссылку и журнал."));
        });
    });
}

int MainWindow::ensureQuattroAutoSelector() {
    const auto group = Configs::dataManager->groupsRepo->CurrentGroup();
    if (!group || group->Profiles().isEmpty()) return -1;

    const QString foreignNameFilter = QStringLiteral(
        R"((?i)^(?!.*(?:🇷🇺|Россия|Россий|Russia|Russian|Москва|МСК|Moscow|Санкт[ -]?Петербург|(?:^|\s|[-|])LTE(?:\s|$|[-|])|⬇|⚠|---)).+$)");
    QList<int> diamondTier;
    QList<int> fastTier;
    bool insideFastSection = false;
    for (const int profileId : group->Profiles()) {
        const auto profile = Configs::dataManager->profilesRepo->GetProfile(profileId);
        if (!profile || profile->type == QStringLiteral("autoselector") || !profile->outbound) continue;
        const auto name = profile->outbound->DisplayName();
        if (isQuattroSubscriptionDecoration(name)) {
            const bool fastHeader = name.contains(QStringLiteral("Быстр"), Qt::CaseInsensitive)
                                    && name.contains(QStringLiteral("10"));
            insideFastSection = fastHeader;
            continue;
        }
        if (isQuattroLteServer(name)) continue;
        if (name.contains(QStringLiteral("💎"))) diamondTier << profileId;
        else if (insideFastSection) fastTier << profileId;
    }
    const QList<QList<int>> quattroPriorityTiers = {diamondTier, fastTier};

    const auto applyQuattroMembershipPolicy = [&](Configs::autoSelector *selector) {
        const bool changed = selector->nameFilter != foreignNameFilter
                             || selector->countryFilter.trimmed().compare(
                                    QStringLiteral("!RU"), Qt::CaseInsensitive) != 0
                             || selector->priorityProfileIds != quattroPriorityTiers
                             || !selector->priorityNameFilters.isEmpty();
        selector->nameFilter = foreignNameFilter;
        selector->countryFilter = QStringLiteral("!RU");
        selector->priorityNameFilters.clear();
        selector->priorityProfileIds = quattroPriorityTiers;
        if (changed) {
            // Never retain a Russian member from a pool ranked before this policy existed.
            selector->pool.clear();
            selector->poolRankedAt = 0;
            selector->lastBuilt.clear();
            selector->lastBuiltAt = 0;
            selector->pinnedID = -1;
        }
    };

    for (const auto &profile : Configs::dataManager->profilesRepo->GetProfileBatch(group->Profiles())) {
        if (profile && profile->type == "autoselector") {
            auto *selector = profile->AutoSelector();
            if (selector) {
                applyQuattroMembershipPolicy(selector);
                // Pick the fastest member on start, then keep it while it is healthy.
                // A very wide hysteresis prevents routine latency jitter from moving
                // sessions; the core still fails over immediately when health drops.
                selector->toleranceMs = 60000;
                selector->watchIntervalSec = 15;
                selector->expected = 3;
                selector->balance = false;
                selector->interruptOnSwitch = true;
                selector->Normalize();
                Configs::dataManager->profilesRepo->Save(profile);
            }
            return profile->id;
        }
    }

    auto profile = Configs::ProfilesRepo::NewProfile(QStringLiteral("autoselector"));
    if (!profile || !profile->AutoSelector()) return -1;
    profile->name = tr("Авто — быстрый зарубежный + failover");
    auto *selector = profile->AutoSelector();
    selector->name = profile->name;
    selector->gid = group->id;
    applyQuattroMembershipPolicy(selector);
    selector->resultValidityMins = 1440;
    selector->intervalSec = 300;
    selector->benchIntervalSec = 600;
    selector->watchIntervalSec = 15;
    selector->expected = 3;
    selector->activeSize = 8;
    selector->toleranceMs = 60000;
    selector->interruptOnSwitch = true;
    selector->balance = false;
    selector->Normalize();
    if (!Configs::dataManager->profilesRepo->AddProfile(profile, group->id)) return -1;
    return profile->id;
}

void MainWindow::setQuattroRussiaBypass(bool enabled) {
    auto &settings = Configs::dataManager->settingsRepo;
    const auto current = Configs::dataManager->routesRepo->GetRouteProfile(settings->current_route_id);
    const bool hasChannels = current && current->name.startsWith(QStringLiteral("Quattro Channels"));
    std::shared_ptr<Configs::RouteProfile> base = enabled ? ensureQuattroRussiaProfile() : nullptr;
    for (const auto &route : Configs::dataManager->routesRepo->GetAllRouteProfiles()) {
        if (!route || route->name.startsWith(QStringLiteral("Quattro Channels"))) continue;
        if (base && enabled) break;
        const bool match = enabled ? route->name.contains("Russia", Qt::CaseInsensitive)
                                   : route->name.compare("Default", Qt::CaseInsensitive) == 0;
        if (match) {
            base = route;
            break;
        }
    }

    if (!base) {
        if (enabled) {
            MessageBoxWarning(tr("Правила российских сервисов"),
                              tr("Не удалось создать встроенный профиль. Проверьте доступ к базе настроек MintVPN."));
        } else {
            MessageBoxWarning(tr("Правила российских сервисов"), tr("Не найден стандартный профиль маршрутизации Default."));
        }
        refreshQuattroDashboard();
        return;
    }

    if (hasChannels) {
        if (base->isRaw) {
            MessageBoxWarning(tr("Правила российских сервисов"),
                              tr("Этот профиль Russia использует необъединяемые raw-правила. Выберите другой профиль Russia в менеджере маршрутов."));
            refreshQuattroDashboard();
            return;
        }
        auto merged = std::make_shared<Configs::RouteProfile>(*base);
        merged->id = current->id;
        merged->name = enabled ? QStringLiteral("Quattro Channels · Bypass Russia")
                               : QStringLiteral("Quattro Channels");
        merged->isRemote = false;
        merged->remoteURL.clear();
        merged->autoUpdate = false;
        QList<std::shared_ptr<Configs::RouteRule>> custom;
        for (const auto &rule : current->Rules) {
            if (rule && rule->name.startsWith(QStringLiteral("Quattro •")))
                custom << std::make_shared<Configs::RouteRule>(*rule);
        }
        merged->Rules = custom + merged->Rules;
        if (!Configs::dataManager->routesRepo->Save(merged)) {
            MessageBoxWarning(tr("Правила российских сервисов"), tr("Не удалось сохранить объединённые правила маршрутизации."));
            refreshQuattroDashboard();
            return;
        }
        settings->current_route_id = merged->id;
    } else {
        settings->current_route_id = base->id;
    }
    settings->Save();
    if (settings->started_id >= 0) profile_start(settings->started_id);
    refreshQuattroDashboard();
}

void MainWindow::showQuattroChannels() {
    const auto current = Configs::dataManager->routesRepo->GetRouteProfile(
        Configs::dataManager->settingsRepo->current_route_id);
    if (current && current->isRaw) {
        MessageBoxWarning(tr("Каналы MintVPN"),
                          tr("Текущий raw-профиль нельзя безопасно объединить с каналами приложений. Выберите Default или структурированный Bypass Russia."));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Маршрутизация сервисов"));
    dialog.resize(820, 620);
    dialog.setMinimumWidth(680);
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(24, 22, 24, 22);
    layout->setSpacing(13);

    auto *hint = new QLabel(tr("Здесь можно настроить правила для российских сайтов, а также "
                               "добавлять отдельные сервисы, сайты и приложения в прямые исключения. "
                               "Google/Gemini при необходимости закрепляется за отдельным VPN-сервером."), &dialog);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *googleRow = new QFormLayout;
    auto *googleServer = new QComboBox(&dialog);
    googleServer->addItem(tr("Как основной VPN"), Configs::proxyID);
    const auto group = Configs::dataManager->groupsRepo->CurrentGroup();
    if (group) {
        for (const auto &profile : Configs::dataManager->profilesRepo->GetProfileBatch(group->Profiles())) {
            if (!profile || profile->type == "autoselector"
                || isQuattroSubscriptionDecoration(profile->name)) continue;
            googleServer->addItem(profile->outbound->DisplayName(), profile->id);
        }
    }
    bool hadChannelSettings = false;
    bool savedSteamDirect = false;
    int savedGoogleServer = Configs::proxyID;
    QList<QuattroDirectService> savedServices;
    bool hasStructuredDirectSettings = false;
    if (current && current->name.startsWith(QStringLiteral("Quattro Channels"))) {
        for (const auto &rule : current->Rules) {
            if (!rule || !rule->name.startsWith(QStringLiteral("Quattro •"))) continue;
            hadChannelSettings = true;
            if (rule->name == QStringLiteral("Quattro • Google geo channel"))
                savedGoogleServer = rule->outboundID;
            if (rule->domain_suffix.contains(QStringLiteral("steampowered.com")) ||
                rule->process_name.contains(QStringLiteral("steam.exe")))
                savedSteamDirect = true;
            if (rule->name.startsWith(kQuattroDirectRulePrefix)) {
                const auto serviceName = rule->name.mid(kQuattroDirectRulePrefix.size());
                if (isRetiredQuattroDirectService(serviceName)) continue;
                hasStructuredDirectSettings = true;
                auto existing = std::find_if(savedServices.begin(), savedServices.end(),
                                             [&serviceName](const QuattroDirectService &service) {
                                                 return service.name.compare(serviceName, Qt::CaseInsensitive) == 0;
                                             });
                if (existing == savedServices.end()) {
                    savedServices << QuattroDirectService{serviceName, rule->domain_suffix, rule->process_name};
                } else {
                    existing->domains << rule->domain_suffix;
                    existing->processes << rule->process_name;
                    existing->domains.removeDuplicates();
                    existing->processes.removeDuplicates();
                }
            }
        }
    }
    if (!hasStructuredDirectSettings) savedServices = defaultQuattroDirectServices();
    const int googleIndex = googleServer->findData(savedGoogleServer);
    if (googleIndex >= 0) googleServer->setCurrentIndex(googleIndex);
    googleRow->addRow(tr("Google / Gemini:"), googleServer);
    layout->addLayout(googleRow);

    auto *steamDirect = new QCheckBox(tr("Steam — напрямую"), &dialog);
    steamDirect->setChecked(hadChannelSettings ? savedSteamDirect : true);
    layout->addWidget(steamDirect);

    auto *directTitle = new QLabel(tr("Российские сервисы напрямую"), &dialog);
    directTitle->setStyleSheet(QStringLiteral("font-weight:600"));
    layout->addWidget(directTitle);

    auto *services = new QTableWidget(&dialog);
    services->setColumnCount(3);
    services->setHorizontalHeaderLabels({tr("Сервис"), tr("Домены через запятую"),
                                          tr("Приложения через запятую")});
    services->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    services->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    services->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    services->verticalHeader()->setVisible(false);
    services->setSelectionBehavior(QAbstractItemView::SelectRows);
    services->setAlternatingRowColors(true);
    const auto appendService = [services](const QuattroDirectService &service) {
        const int row = services->rowCount();
        services->insertRow(row);
        services->setItem(row, 0, new QTableWidgetItem(service.name));
        services->setItem(row, 1, new QTableWidgetItem(service.domains.join(QStringLiteral(", "))));
        services->setItem(row, 2, new QTableWidgetItem(service.processes.join(QStringLiteral(", "))));
    };
    for (const auto &service : savedServices) appendService(service);
    layout->addWidget(services, 1);

    auto *serviceButtons = new QHBoxLayout;
    auto *addService = new QPushButton(tr("Добавить сервис"), &dialog);
    auto *removeService = new QPushButton(tr("Удалить выбранный"), &dialog);
    serviceButtons->addWidget(addService);
    serviceButtons->addWidget(removeService);
    serviceButtons->addStretch();
    connect(addService, &QPushButton::clicked, &dialog, [services, appendService] {
        appendService({QObject::tr("Новый сервис"), {}, {}});
        services->setCurrentCell(services->rowCount() - 1, 0);
        services->editItem(services->currentItem());
    });
    connect(removeService, &QPushButton::clicked, &dialog, [services] {
        if (services->currentRow() >= 0) services->removeRow(services->currentRow());
    });
    layout->addLayout(serviceButtons);

    auto *protectedServices = new QLabel(tr("Через основной VPN: ChatGPT/OpenAI, Microsoft/Copilot, Telegram, Adobe"), &dialog);
    protectedServices->setStyleSheet(QStringLiteral("color:#5f6670"));
    protectedServices->setWordWrap(true);
    layout->addWidget(protectedServices);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Save)->setText(tr("Применить"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Отмена"));
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) return;

    const bool updateExisting = current && current->name.startsWith(QStringLiteral("Quattro Channels"));
    const bool inheritsRussiaBypass = current && current->name.contains("Russia", Qt::CaseInsensitive);
    auto channels = current ? std::make_shared<Configs::RouteProfile>(*current)
                            : Configs::RouteProfile::GetDefaultChain();
    channels->id = updateExisting ? current->id : -1;
    channels->name = inheritsRussiaBypass
                         ? QStringLiteral("Quattro Channels · Bypass Russia")
                         : QStringLiteral("Quattro Channels");
    channels->isRemote = false;
    channels->remoteURL.clear();
    channels->autoUpdate = false;

    QList<std::shared_ptr<Configs::RouteRule>> custom;
    if (steamDirect->isChecked()) {
        auto directDomains = std::make_shared<Configs::RouteRule>();
        directDomains->name = QStringLiteral("Quattro • Steam domains");
        directDomains->type = Configs::simpleAddressBypass;
        directDomains->action = QStringLiteral("route");
        directDomains->outboundID = Configs::directID;
        directDomains->domain_suffix << "steampowered.com" << "steamcommunity.com"
                                     << "steamstatic.com" << "steamcontent.com";
        custom << directDomains;
    }

    if (steamDirect->isChecked()) {
        auto directProcesses = std::make_shared<Configs::RouteRule>();
        directProcesses->name = QStringLiteral("Quattro • Direct applications");
        directProcesses->type = Configs::simpleProcessNameBypass;
        directProcesses->action = QStringLiteral("route");
        directProcesses->outboundID = Configs::directID;
        directProcesses->process_name << "steam.exe" << "steamwebhelper.exe";
        custom << directProcesses;
    }

    for (int row = 0; row < services->rowCount(); ++row) {
        const auto nameItem = services->item(row, 0);
        const auto domainsItem = services->item(row, 1);
        const auto processesItem = services->item(row, 2);
        const auto name = nameItem ? nameItem->text().trimmed() : QString();
        const auto domains = splitRoutingValues(domainsItem ? domainsItem->text() : QString());
        const auto processes = splitRoutingValues(processesItem ? processesItem->text() : QString());
        if (name.isEmpty() || (domains.isEmpty() && processes.isEmpty())) continue;
        if (!domains.isEmpty()) {
            auto directDomains = std::make_shared<Configs::RouteRule>();
            directDomains->name = kQuattroDirectRulePrefix + name;
            directDomains->type = Configs::simpleAddressBypass;
            directDomains->action = QStringLiteral("route");
            directDomains->outboundID = Configs::directID;
            directDomains->domain_suffix = domains;
            custom << directDomains;
        }
        if (!processes.isEmpty()) {
            auto directProcesses = std::make_shared<Configs::RouteRule>();
            directProcesses->name = kQuattroDirectRulePrefix + name;
            directProcesses->type = Configs::simpleProcessNameBypass;
            directProcesses->action = QStringLiteral("route");
            directProcesses->outboundID = Configs::directID;
            directProcesses->process_name = processes;
            custom << directProcesses;
        }
    }

    auto google = std::make_shared<Configs::RouteRule>();
    google->name = QStringLiteral("Quattro • Google geo channel");
    google->type = Configs::simpleAddressProxy;
    google->action = QStringLiteral("route");
    google->outboundID = googleServer->currentData().toInt();
    // Only Gemini and YouTube: Search, Drive and Docs stay direct.
    google->domain_suffix = quattroGoogleChannelDomains();
    custom << google;

    QList<std::shared_ptr<Configs::RouteRule>> inherited;
    for (const auto &rule : channels->Rules) {
        if (!rule || rule->name.startsWith("Quattro •")) continue;
        inherited << rule;
    }
    channels->Rules = custom + inherited;
    const bool saved = updateExisting
                           ? Configs::dataManager->routesRepo->Save(channels)
                           : Configs::dataManager->routesRepo->AddRouteProfile(channels);
    if (!saved) {
        MessageBoxWarning(tr("Каналы MintVPN"), tr("Не удалось сохранить профиль маршрутизации."));
        return;
    }
    Configs::dataManager->settingsRepo->current_route_id = channels->id;
    Configs::dataManager->settingsRepo->Save();
    if (Configs::dataManager->settingsRepo->started_id >= 0)
        profile_start(Configs::dataManager->settingsRepo->started_id);
    refreshQuattroDashboard();
}
