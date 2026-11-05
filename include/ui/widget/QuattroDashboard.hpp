#pragma once

#include <QWidget>

class QCheckBox;
class QComboBox;
class QFrame;
class QGridLayout;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QResizeEvent;

class QuattroDashboard final : public QWidget {
    Q_OBJECT

public:
    enum class Mode { Tun = 0, SystemProxy = 1 };

    explicit QuattroDashboard(QWidget *parent = nullptr);

    void setConnectionState(bool connected, const QString &status, const QString &server = {},
                            bool transitioning = false);
    void setProfiles(const QList<QPair<int, QString>> &profiles, int selectedId, bool includeAuto = true);
    void setSelectedProfile(int profileId);
    void setMode(Mode mode);
    void setRussiaBypass(bool enabled);
    void setAutoStart(bool enabled);
    void setSubscriptionState(bool busy, const QString &message = {});
    QString subscriptionUrl() const;

signals:
    void connectionToggled(bool connect);
    void subscriptionSubmitted(const QString &url);
    void subscriptionRefreshRequested();
    void modeChanged(QuattroDashboard::Mode mode);
    void profileChanged(int profileId);
    void russiaBypassChanged(bool enabled);
    void autoStartChanged(bool enabled);
    void channelsRequested();
    void routingRequested();
    void settingsRequested();
    void recoverRequested();
    void advancedRequested();

private:
    void resizeEvent(QResizeEvent *event) override;

    bool m_connected = false;
    QLabel *m_statusDot = nullptr;
    QLabel *m_statusText = nullptr;
    QLabel *m_activeServer = nullptr;
    QPushButton *m_connectButton = nullptr;
    QPushButton *m_recoverButton = nullptr;
    QLineEdit *m_subscription = nullptr;
    QLabel *m_subscriptionStatus = nullptr;
    QPushButton *m_subscriptionSave = nullptr;
    QPushButton *m_subscriptionRefresh = nullptr;
    QPushButton *m_autoProfile = nullptr;
    QComboBox *m_profiles = nullptr;
    QPushButton *m_tunButton = nullptr;
    QPushButton *m_proxyButton = nullptr;
    QCheckBox *m_russiaBypass = nullptr;
    QCheckBox *m_autoStart = nullptr;
    QFrame *m_connectionCard = nullptr;
    QFrame *m_routingCard = nullptr;
    QGridLayout *m_cardsLayout = nullptr;
    QGridLayout *m_subscriptionActions = nullptr;
    QHBoxLayout *m_footerLayout = nullptr;
    QLabel *m_footerEngine = nullptr;
    bool m_hasProfiles = false;
    bool m_transitioning = false;
    bool m_compactLayout = false;

    void applyModeStyle();
    void applyResponsiveLayout(bool force = false);
    void updateConnectionButton();
};
