#include "include/ui/widget/QuattroDashboard.hpp"

#include <algorithm>
#include <QButtonGroup>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStandardItemModel>
#include <QStyle>
#include <QVBoxLayout>

namespace {
QFrame *card(QWidget *parent = nullptr) {
    auto *frame = new QFrame(parent);
    frame->setObjectName(QStringLiteral("card"));
    frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    return frame;
}

QPushButton *secondaryButton(const QString &text, QWidget *parent = nullptr) {
    auto *button = new QPushButton(text, parent);
    button->setProperty("kind", "secondary");
    button->setCursor(Qt::PointingHandCursor);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    return button;
}

QLabel *eyebrow(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("eyebrow"));
    return label;
}
}

QuattroDashboard::QuattroDashboard(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("quattroDashboard"));
    setMinimumSize(600, 520);

    auto *page = new QWidget(this);
    page->setObjectName(QStringLiteral("dashboardPage"));
    page->setMinimumWidth(0);
    page->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    auto *root = new QVBoxLayout(page);
    root->setContentsMargins(28, 24, 28, 24);
    root->setSpacing(14);

    auto *header = card(page);
    header->setObjectName(QStringLiteral("headerCard"));
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(18, 14, 18, 14);
    headerLayout->setSpacing(13);

    auto *logo = new QLabel(header);
    logo->setPixmap(QPixmap(QStringLiteral(":/Quattro/Quattro.png")).scaled(
        38, 38, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setFixedSize(40, 40);
    headerLayout->addWidget(logo);

    auto *brand = new QVBoxLayout;
    brand->setSpacing(0);
    auto *title = new QLabel(QStringLiteral("MintVPN"), header);
    title->setObjectName(QStringLiteral("brandTitle"));
    brand->addWidget(title);
    headerLayout->addLayout(brand);
    headerLayout->addStretch();

    auto *statusPill = new QFrame(header);
    statusPill->setObjectName(QStringLiteral("statusPill"));
    auto *statusLayout = new QHBoxLayout(statusPill);
    statusLayout->setContentsMargins(12, 7, 12, 7);
    statusLayout->setSpacing(7);
    m_statusDot = new QLabel(QStringLiteral("●"), statusPill);
    m_statusDot->setObjectName(QStringLiteral("statusDot"));
    m_statusText = new QLabel(tr("Отключено"), statusPill);
    m_statusText->setObjectName(QStringLiteral("statusText"));
    statusLayout->addWidget(m_statusDot);
    statusLayout->addWidget(m_statusText);
    headerLayout->addWidget(statusPill);
    root->addWidget(header);

    auto *hero = card(page);
    hero->setObjectName(QStringLiteral("heroCard"));
    auto *heroLayout = new QHBoxLayout(hero);
    heroLayout->setContentsMargins(24, 22, 24, 22);
    heroLayout->setSpacing(20);

    auto *heroText = new QVBoxLayout;
    heroText->setSpacing(7);
    heroText->addWidget(eyebrow(tr("УМНОЕ ПОДКЛЮЧЕНИЕ"), hero));
    auto *heroTitle = new QLabel(tr("Подключение и маршруты"), hero);
    heroTitle->setObjectName(QStringLiteral("sectionTitle"));
    heroTitle->setWordWrap(true);
    auto *heroHint = new QLabel(
        tr("Настройте подключение и правила для сервисов и приложений."), hero);
    heroHint->setObjectName(QStringLiteral("muted"));
    heroHint->setWordWrap(true);
    m_activeServer = new QLabel(tr("Авто • выбор сервера"), hero);
    m_activeServer->setObjectName(QStringLiteral("serverLabel"));
    m_activeServer->setWordWrap(true);
    heroText->addWidget(heroTitle);
    heroText->addWidget(heroHint);
    heroText->addSpacing(4);
    heroText->addWidget(m_activeServer);
    heroLayout->addLayout(heroText, 1);

    m_connectButton = new QPushButton(tr("Подключить"), hero);
    m_connectButton->setObjectName(QStringLiteral("connectButton"));
    m_connectButton->setCursor(Qt::PointingHandCursor);
    m_connectButton->setMinimumWidth(168);
    m_connectButton->setFixedHeight(52);
    connect(m_connectButton, &QPushButton::clicked, this, [this] {
        emit connectionToggled(!m_connected);
    });
    heroLayout->addWidget(m_connectButton, 0, Qt::AlignVCenter);
    root->addWidget(hero);

    m_cardsLayout = new QGridLayout;
    m_cardsLayout->setContentsMargins(0, 0, 0, 0);
    m_cardsLayout->setHorizontalSpacing(14);
    m_cardsLayout->setVerticalSpacing(14);

    m_connectionCard = card(page);
    auto *connectionLayout = new QVBoxLayout(m_connectionCard);
    connectionLayout->setContentsMargins(20, 18, 20, 18);
    connectionLayout->setSpacing(11);
    connectionLayout->addWidget(eyebrow(tr("КАНАЛ"), m_connectionCard));
    auto *connectionTitle = new QLabel(tr("Подключение"), m_connectionCard);
    connectionTitle->setObjectName(QStringLiteral("cardTitle"));
    connectionLayout->addWidget(connectionTitle);

    auto *modeSelector = new QFrame(m_connectionCard);
    modeSelector->setObjectName(QStringLiteral("modeSelector"));
    auto *modeRow = new QHBoxLayout(modeSelector);
    modeRow->setContentsMargins(4, 4, 4, 4);
    modeRow->setSpacing(4);
    m_tunButton = secondaryButton(QStringLiteral("TUN"), m_connectionCard);
    m_proxyButton = secondaryButton(tr("Системный прокси"), m_connectionCard);
    m_tunButton->setProperty("kind", "segment");
    m_proxyButton->setProperty("kind", "segment");
    m_tunButton->setCheckable(true);
    m_proxyButton->setCheckable(true);
    auto *modeGroup = new QButtonGroup(this);
    modeGroup->setExclusive(true);
    modeGroup->addButton(m_tunButton);
    modeGroup->addButton(m_proxyButton);
    modeRow->addWidget(m_tunButton);
    modeRow->addWidget(m_proxyButton);
    connectionLayout->addWidget(modeSelector);
    connect(m_tunButton, &QPushButton::clicked, this, [this] { emit modeChanged(Mode::Tun); });
    connect(m_proxyButton, &QPushButton::clicked, this, [this] { emit modeChanged(Mode::SystemProxy); });

    auto *serverCaption = new QLabel(tr("Как подключаться"), m_connectionCard);
    serverCaption->setObjectName(QStringLiteral("fieldLabel"));
    auto *serverSelector = new QFrame(m_connectionCard);
    serverSelector->setObjectName(QStringLiteral("serverSelector"));
    auto *serverRow = new QHBoxLayout(serverSelector);
    serverRow->setContentsMargins(4, 4, 4, 4);
    serverRow->setSpacing(4);

    m_autoProfile = secondaryButton(tr("⚡ Авто"), serverSelector);
    m_autoProfile->setObjectName(QStringLiteral("autoServerButton"));
    m_autoProfile->setProperty("kind", "serverChoice");
    m_autoProfile->setCheckable(true);
    m_autoProfile->setToolTip(tr("MintVPN сам выберет сервер по приоритету и скорости"));

    m_profiles = new QComboBox(m_connectionCard);
    m_profiles->setPlaceholderText(tr("Выбрать сервер из списка…"));
    m_profiles->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_profiles, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index < 0) return;
        const int profileId = m_profiles->itemData(index).toInt();
        if (profileId < 0) return;
        {
            const QSignalBlocker blocker(m_autoProfile);
            m_autoProfile->setChecked(false);
        }
        m_activeServer->setText(tr("Выбран • %1").arg(m_profiles->itemText(index)));
        emit profileChanged(profileId);
    });
    connect(m_autoProfile, &QPushButton::clicked, this, [this] {
        {
            const QSignalBlocker profileBlocker(m_profiles);
            m_profiles->setCurrentIndex(-1);
        }
        {
            const QSignalBlocker autoBlocker(m_autoProfile);
            m_autoProfile->setChecked(true);
        }
        m_activeServer->setText(tr("Авто • выбор сервера"));
        emit profileChanged(-1);
    });
    serverRow->addWidget(m_autoProfile);
    serverRow->addWidget(m_profiles, 1);
    connectionLayout->addWidget(serverCaption);
    connectionLayout->addWidget(serverSelector);

    m_routingCard = card(page);
    auto *routingLayout = new QVBoxLayout(m_routingCard);
    routingLayout->setContentsMargins(20, 18, 20, 18);
    routingLayout->setSpacing(9);
    routingLayout->addWidget(eyebrow(tr("ПРАВИЛА"), m_routingCard));
    auto *routingTitle = new QLabel(tr("Маршрутизация"), m_routingCard);
    routingTitle->setObjectName(QStringLiteral("cardTitle"));
    routingLayout->addWidget(routingTitle);
    m_russiaBypass = new QCheckBox(tr("Правила российских сервисов"), m_routingCard);
    m_russiaBypass->setToolTip(tr("Настроить правила маршрутизации российских доменов и IP"));
    m_autoStart = new QCheckBox(tr("Автозапуск и подключение"), m_routingCard);
    m_autoStart->setToolTip(tr("MintVPN запустится в трее и восстановит режим и сервер"));
    routingLayout->addWidget(m_russiaBypass);
    routingLayout->addWidget(m_autoStart);
    auto *localDirect = new QLabel(tr("Локальная сеть и Tailscale: маршрут не проверен"), m_routingCard);
    localDirect->setObjectName(QStringLiteral("localDirect"));
    localDirect->setWordWrap(true);
    localDirect->setToolTip(tr("Фактический маршрут локальной сети и Tailscale ещё не проверен"));
    routingLayout->addWidget(localDirect);
    auto *routingButtons = new QHBoxLayout;
    routingButtons->setSpacing(8);
    auto *channels = secondaryButton(tr("Сервисы"), m_routingCard);
    auto *routes = secondaryButton(tr("Исключения"), m_routingCard);
    routingButtons->addWidget(channels);
    routingButtons->addWidget(routes);
    routingLayout->addLayout(routingButtons);
    connect(m_russiaBypass, &QCheckBox::toggled, this, &QuattroDashboard::russiaBypassChanged);
    connect(m_autoStart, &QCheckBox::toggled, this, &QuattroDashboard::autoStartChanged);
    connect(channels, &QPushButton::clicked, this, &QuattroDashboard::channelsRequested);
    connect(routes, &QPushButton::clicked, this, &QuattroDashboard::routingRequested);

    m_cardsLayout->addWidget(m_connectionCard, 0, 0);
    m_cardsLayout->addWidget(m_routingCard, 0, 1);
    m_cardsLayout->setColumnStretch(0, 1);
    m_cardsLayout->setColumnStretch(1, 1);
    root->addLayout(m_cardsLayout);

    auto *subCard = card(page);
    auto *subLayout = new QVBoxLayout(subCard);
    subLayout->setContentsMargins(20, 17, 20, 17);
    subLayout->setSpacing(10);
    auto *subHead = new QHBoxLayout;
    auto *subText = new QVBoxLayout;
    subText->setSpacing(2);
    auto *subTitle = new QLabel(tr("Подписка VPN"), subCard);
    subTitle->setObjectName(QStringLiteral("cardTitle"));
    auto *subHint = new QLabel(tr("Вставьте ссылку один раз — серверы обновятся автоматически."), subCard);
    subHint->setObjectName(QStringLiteral("muted"));
    subHint->setWordWrap(true);
    subText->addWidget(subTitle);
    subText->addWidget(subHint);
    subHead->addLayout(subText, 1);
    m_subscriptionStatus = new QLabel(subCard);
    m_subscriptionStatus->setObjectName(QStringLiteral("statusBadge"));
    m_subscriptionStatus->setWordWrap(true);
    subHead->addWidget(m_subscriptionStatus, 0, Qt::AlignTop);
    subLayout->addLayout(subHead);

    m_subscriptionActions = new QGridLayout;
    m_subscriptionActions->setContentsMargins(0, 0, 0, 0);
    m_subscriptionActions->setHorizontalSpacing(8);
    m_subscriptionActions->setVerticalSpacing(8);
    m_subscription = new QLineEdit(subCard);
    m_subscription->setPlaceholderText(QStringLiteral("https://…"));
    m_subscription->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    m_subscription->setMinimumWidth(0);
    m_subscriptionSave = new QPushButton(tr("Добавить"), subCard);
    m_subscriptionSave->setObjectName(QStringLiteral("primaryButton"));
    m_subscriptionRefresh = secondaryButton(tr("Обновить"), subCard);
    m_subscriptionActions->addWidget(m_subscription, 0, 0);
    m_subscriptionActions->addWidget(m_subscriptionSave, 0, 1);
    m_subscriptionActions->addWidget(m_subscriptionRefresh, 0, 2);
    m_subscriptionActions->setColumnStretch(0, 1);
    subLayout->addLayout(m_subscriptionActions);
    connect(m_subscriptionSave, &QPushButton::clicked, this, [this] {
        emit subscriptionSubmitted(m_subscription->text().trimmed());
    });
    connect(m_subscription, &QLineEdit::returnPressed, m_subscriptionSave, &QPushButton::click);
    connect(m_subscriptionRefresh, &QPushButton::clicked, this, &QuattroDashboard::subscriptionRefreshRequested);
    root->addWidget(subCard);

    m_footerLayout = new QHBoxLayout;
    m_footerLayout->setSpacing(8);
    m_footerEngine = new QLabel(tr("MintVPN • sing-box"), page);
    m_footerEngine->setObjectName(QStringLiteral("muted"));
    m_footerLayout->addWidget(m_footerEngine);
    m_footerLayout->addStretch();
    auto *settings = secondaryButton(tr("Настройки"), page);
    settings->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto *advanced = secondaryButton(tr("Расширенный режим"), page);
    advanced->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_recoverButton = secondaryButton(tr("Восстановить сеть"), page);
    m_recoverButton->setToolTip(tr("Очистить DNS-кэш Windows и переподключить активный VPN. Текущие соединения прервутся."));
    m_footerLayout->addWidget(m_recoverButton);
    connect(m_recoverButton, &QPushButton::clicked, this, &QuattroDashboard::recoverRequested);
    m_footerLayout->addWidget(settings);
    m_footerLayout->addWidget(advanced);
    connect(settings, &QPushButton::clicked, this, &QuattroDashboard::settingsRequested);
    connect(advanced, &QPushButton::clicked, this, &QuattroDashboard::advancedRequested);
    root->addLayout(m_footerLayout);

    auto *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("dashboardScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setWidget(page);
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scroll);

    setStyleSheet(QStringLiteral(R"qss(
        QWidget#quattroDashboard, QWidget#dashboardPage, QScrollArea#dashboardScroll,
        QScrollArea#dashboardScroll > QWidget > QWidget {
            background: #141a23;
            color: #f5f7fb;
            font-family: "Segoe UI Variable Text", "Segoe UI";
            font-size: 14px;
        }
        QFrame#card, QFrame#headerCard {
            background: #1b222c;
            border: 1px solid #303946;
            border-radius: 12px;
        }
        QFrame#headerCard { background: #171e27; }
        QFrame#heroCard {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                        stop:0 #202633, stop:1 #19232d);
            border: 1px solid #34404d;
            border-radius: 14px;
        }
        QFrame#statusPill {
            background: #202833;
            border: 1px solid #3a4553;
            border-radius: 15px;
        }
        QLabel#brandTitle { color: #ffffff; font-size: 21px; font-weight: 750; }
        QLabel#sectionTitle { color: #ffffff; font-size: 25px; font-weight: 750; }
        QLabel#cardTitle { color: #f7f9fc; font-size: 17px; font-weight: 700; }
        QLabel#eyebrow { color: #58d8e8; font-size: 11px; font-weight: 700; letter-spacing: 1px; }
        QLabel#fieldLabel { color: #9ca6b4; font-size: 12px; }
        QLabel#muted { color: #aeb6c2; }
        QLabel#serverLabel { color: #67dce9; font-weight: 650; }
        QLabel#statusDot { color: #697586; }
        QLabel#statusText { color: #e8ecf2; font-weight: 650; }
        QLabel#statusBadge { color: #67dce9; font-size: 12px; font-weight: 600; }
        QLabel#localDirect {
            color: #b6c1ce;
            background: #202a36;
            border: 1px solid #3a4858;
            border-radius: 9px;
            padding: 8px 10px;
            font-size: 12px;
            font-weight: 600;
        }
        QFrame#modeSelector {
            background: #131922;
            border: 1px solid #303946;
            border-radius: 11px;
        }
        QFrame#serverSelector {
            background: #131922;
            border: 1px solid #303946;
            border-radius: 11px;
        }
        QPushButton {
            min-height: 42px;
            border-radius: 10px;
            padding: 0 16px;
            font-weight: 650;
        }
        QPushButton[kind="secondary"] {
            background: #222a35;
            border: 1px solid #3a4553;
            color: #e5eaf0;
        }
        QPushButton[kind="secondary"]:hover { background: #2a3542; border-color: #607084; color: #ffffff; }
        QPushButton[kind="secondary"]:pressed { background: #171e27; border-color: #31bdd0; }
        QPushButton[kind="secondary"]:checked {
            background: #173c48;
            border-color: #35c7dc;
            color: #72e6f3;
        }
        QPushButton[kind="segment"] {
            min-height: 36px;
            background: transparent;
            border: none;
            border-radius: 8px;
            color: #9da8b6;
        }
        QPushButton[kind="segment"]:hover { background: #1c2530; color: #eef3f7; }
        QPushButton[kind="segment"]:checked {
            background: #1b414b;
            border: 1px solid #32bfd2;
            color: #70e2ee;
        }
        QPushButton[kind="serverChoice"] {
            min-height: 40px;
            background: transparent;
            border: none;
            border-radius: 8px;
            color: #aeb8c5;
            padding: 0 13px;
        }
        QPushButton[kind="serverChoice"]:hover { background: #1c2530; color: #ffffff; }
        QPushButton[kind="serverChoice"]:checked {
            background: #173c48;
            border: 1px solid #35c7dc;
            color: #72e6f3;
        }
        QPushButton#primaryButton {
            background: #2abed1;
            border: 1px solid #4ad1e2;
            color: #07171b;
            font-weight: 750;
        }
        QPushButton#primaryButton:hover { background: #51d4e3; border-color: #72e2ee; }
        QPushButton#primaryButton:pressed { background: #2198a8; }
        QPushButton#connectButton {
            background: #2abed1;
            border: 1px solid #55d7e6;
            color: #07171b;
            font-size: 15px;
            font-weight: 750;
        }
        QPushButton#connectButton:hover { background: #52d6e5; border-color: #7be7f0; }
        QPushButton#connectButton:pressed { background: #209aa9; }
        QPushButton#connectButton:disabled { background: #343b46; border-color: #49515d; color: #858e9b; }
        QPushButton#connectButton[connected="true"] {
            background: #3a2327;
            border-color: #d9545b;
            color: #ff9ba0;
        }
        QPushButton#connectButton[connected="true"]:hover { background: #4a292e; border-color: #f16b72; }
        QLineEdit, QComboBox {
            min-height: 40px;
            background: #151b24;
            border: 1px solid #3a4553;
            border-radius: 9px;
            padding: 0 12px;
            color: #f4f6f9;
            selection-background-color: #1f8291;
        }
        QLineEdit:focus, QComboBox:focus { border-color: #3bd0e3; }
        QLineEdit:disabled, QComboBox:disabled { color: #727d8b; background: #191f28; }
        QComboBox::drop-down { border: none; width: 28px; }
        QComboBox QAbstractItemView {
            background: #1b222c;
            color: #f5f7fb;
            border: 1px solid #3a4553;
            selection-background-color: #17424d;
        }
        QCheckBox {
            min-height: 28px;
            spacing: 10px;
            color: #dfe5ec;
            background: #171e27;
            border: 1px solid #303946;
            border-radius: 9px;
            padding: 7px 10px;
        }
        QCheckBox:hover { background: #1d2631; border-color: #4b596b; }
        QCheckBox:checked { background: #17343d; border-color: #2dbed1; color: #71e3ef; }
        QCheckBox::indicator { width: 18px; height: 18px; }
        QCheckBox::indicator:unchecked { image: url(:/qss_icons/dark/rc/checkbox_unchecked@2x.png); }
        QCheckBox::indicator:checked { image: url(:/qss_icons/dark/rc/checkbox_checked@2x.png); }
        QScrollBar:vertical { background: #141a23; width: 8px; margin: 2px; }
        QScrollBar::handle:vertical { background: #3d4856; border-radius: 4px; min-height: 28px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
    )qss"));

    setMode(Mode::Tun);
    setConnectionState(false, tr("Отключено"));
    applyResponsiveLayout(true);
}

void QuattroDashboard::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    applyResponsiveLayout();
}

void QuattroDashboard::applyResponsiveLayout(bool force) {
    const bool compact = width() < 780;
    if (!force && compact == m_compactLayout) return;
    m_compactLayout = compact;

    m_cardsLayout->removeWidget(m_connectionCard);
    m_cardsLayout->removeWidget(m_routingCard);
    m_subscriptionActions->removeWidget(m_subscription);
    m_subscriptionActions->removeWidget(m_subscriptionSave);
    m_subscriptionActions->removeWidget(m_subscriptionRefresh);

    if (compact) {
        m_cardsLayout->addWidget(m_connectionCard, 0, 0, 1, 2);
        m_cardsLayout->addWidget(m_routingCard, 1, 0, 1, 2);
        m_subscriptionActions->addWidget(m_subscription, 0, 0, 1, 2);
        m_subscriptionActions->addWidget(m_subscriptionSave, 1, 0);
        m_subscriptionActions->addWidget(m_subscriptionRefresh, 1, 1);
        m_footerEngine->hide();
    } else {
        m_cardsLayout->addWidget(m_connectionCard, 0, 0);
        m_cardsLayout->addWidget(m_routingCard, 0, 1);
        m_subscriptionActions->addWidget(m_subscription, 0, 0);
        m_subscriptionActions->addWidget(m_subscriptionSave, 0, 1);
        m_subscriptionActions->addWidget(m_subscriptionRefresh, 0, 2);
        m_footerEngine->show();
    }
}

void QuattroDashboard::setConnectionState(bool connected, const QString &status, const QString &server,
                                          bool transitioning) {
    m_connected = connected;
    m_transitioning = transitioning;
    m_tunButton->setEnabled(!transitioning);
    m_proxyButton->setEnabled(!transitioning);
    m_recoverButton->setEnabled(!transitioning);
    m_statusText->setText(status);
    m_statusDot->setStyleSheet(connected ? QStringLiteral("color:#35d08a")
                                         : QStringLiteral("color:#697586"));
    m_connectButton->setText(transitioning ? status
                                          : connected ? tr("Отключить") : tr("Подключить"));
    m_connectButton->setProperty("connected", connected);
    m_connectButton->style()->unpolish(m_connectButton);
    m_connectButton->style()->polish(m_connectButton);
    m_activeServer->setText(server.isEmpty() ? tr("Авто • выбор сервера")
                                             : tr("Сервер • %1").arg(server));
    updateConnectionButton();
}

void QuattroDashboard::setProfiles(const QList<QPair<int, QString>> &profiles, int selectedId, bool includeAuto) {
    const QSignalBlocker profileBlocker(m_profiles);
    const QSignalBlocker autoBlocker(m_autoProfile);
    m_profiles->clear();
    m_hasProfiles = std::any_of(profiles.cbegin(), profiles.cend(), [](const auto &profile) {
        return profile.first >= 0;
    });
    m_profiles->setPlaceholderText(m_hasProfiles ? tr("Выбрать сервер из списка…")
                                                 : tr("Сначала добавьте подписку"));
    for (const auto &[id, name] : profiles) {
        m_profiles->addItem(name, id);
        if (id >= 0) continue;
        if (auto *model = qobject_cast<QStandardItemModel *>(m_profiles->model())) {
            if (auto *item = model->item(m_profiles->count() - 1)) {
                item->setFlags(item->flags() & ~Qt::ItemIsEnabled & ~Qt::ItemIsSelectable);
                item->setData(QColor(QStringLiteral("#67dce9")), Qt::ForegroundRole);
            }
        }
    }
    const int index = m_profiles->findData(selectedId);
    const bool autoSelected = includeAuto && selectedId < 0;
    m_autoProfile->setVisible(includeAuto);
    m_autoProfile->setEnabled(m_hasProfiles);
    m_autoProfile->setChecked(autoSelected);
    m_profiles->setCurrentIndex(autoSelected ? -1 : index);
    m_profiles->setEnabled(m_hasProfiles);
    if (autoSelected) {
        m_activeServer->setText(tr("Авто • выбор сервера"));
    } else if (index >= 0) {
        m_activeServer->setText(tr("Выбран • %1").arg(m_profiles->itemText(index)));
    }
    updateConnectionButton();
}

void QuattroDashboard::setSelectedProfile(int profileId) {
    const QSignalBlocker profileBlocker(m_profiles);
    const QSignalBlocker autoBlocker(m_autoProfile);
    const bool autoSelected = profileId < 0;
    m_autoProfile->setChecked(autoSelected);
    if (autoSelected) {
        m_profiles->setCurrentIndex(-1);
        m_activeServer->setText(tr("Авто • выбор сервера"));
        return;
    }
    const int index = m_profiles->findData(profileId);
    if (index >= 0) {
        m_profiles->setCurrentIndex(index);
        m_activeServer->setText(tr("Выбран • %1").arg(m_profiles->itemText(index)));
    }
}

void QuattroDashboard::setMode(Mode mode) {
    const QSignalBlocker tunBlock(m_tunButton);
    const QSignalBlocker proxyBlock(m_proxyButton);
    m_tunButton->setChecked(mode == Mode::Tun);
    m_proxyButton->setChecked(mode == Mode::SystemProxy);
    applyModeStyle();
}

void QuattroDashboard::setRussiaBypass(bool enabled) {
    const QSignalBlocker blocker(m_russiaBypass);
    m_russiaBypass->setChecked(enabled);
}

void QuattroDashboard::setAutoStart(bool enabled) {
    const QSignalBlocker blocker(m_autoStart);
    m_autoStart->setChecked(enabled);
}

void QuattroDashboard::setSubscriptionState(bool busy, const QString &message) {
    m_subscription->setEnabled(!busy);
    m_subscriptionSave->setEnabled(!busy);
    m_subscriptionRefresh->setEnabled(!busy);
    m_subscriptionSave->setText(busy ? tr("Загрузка…") : tr("Добавить"));
    m_subscriptionStatus->setText(message);
}

QString QuattroDashboard::subscriptionUrl() const { return m_subscription->text().trimmed(); }

void QuattroDashboard::applyModeStyle() {
    for (auto *button : {m_tunButton, m_proxyButton}) {
        button->style()->unpolish(button);
        button->style()->polish(button);
    }
}

void QuattroDashboard::updateConnectionButton() {
    m_connectButton->setEnabled(!m_transitioning && (m_connected || m_hasProfiles));
}
