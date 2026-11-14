// --ui-tour <dir>: walks the new interface in each of its themes and saves what the screen shows, for CI.
// The whole screen is grabbed, so window corners, shadows and translucency come out as the user sees them.
#include "include/ui/widget/NewHome.hpp"

#include "include/ui/mainwindow.h"
#include "include/ui/setting/ThemeManager.hpp"
#include "include/configs/sub/GroupUpdater.hpp"
#include "include/database/SettingsRepo.h"
#include "include/engine/EngineHost.h"

#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QMenu>
#include <QScreen>
#include <QTabWidget>
#include <QTimer>

#include <functional>
#include <memory>

namespace {
    // Sample servers on documentation addresses: rows to look at, nothing to connect to.
    const char *kSampleLinks =
        "vless://11111111-1111-1111-1111-111111111111@203.0.113.10:443?security=reality&type=tcp&sni=example.com&pbk=YWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXoxMjM0NTY&sid=01#%F0%9F%87%B8%F0%9F%87%AA%20se-tcp-reality\n"
        "vless://11111111-1111-1111-1111-111111111111@203.0.113.11:443?security=tls&type=xhttp&sni=example.com#%F0%9F%87%B7%F0%9F%87%BA%20ru-xhttp-tls\n"
        "hysteria2://secret@203.0.113.12:443?sni=example.com#%F0%9F%87%A9%F0%9F%87%AA%20de-hy2\n"
        "ss://Y2hhY2hhMjAtaWV0Zi1wb2x5MTMwNTpzZWNyZXQ@203.0.113.13:8388#nl-ss\n";

    struct Tour {
        MainWindow *mw;
        QDir dir;
        QList<std::pair<QString, std::function<void()>>> steps;
        int at = 0;
        QString theme;

        void shoot(const QString &name) {
            const QString file = dir.filePath(QStringLiteral("%1-%2-%3.png").arg(theme).arg(at, 3, 10, QLatin1Char('0')).arg(name));
            if (QScreen *screen = QGuiApplication::primaryScreen()) screen->grabWindow(0).save(file);
        }
        // Everything the step opened goes away before the next one: popups, menus and dialog windows.
        void clear() {
            for (int pass = 0; pass < 3; ++pass) {
                if (QWidget *popup = QApplication::activePopupWidget()) { popup->close(); continue; }
                bool closed = false;
                for (QWidget *w : QApplication::topLevelWidgets()) {
                    if (w == mw || !w->isVisible()) continue;
                    if (auto *dialog = qobject_cast<QDialog *>(w)) dialog->reject();
                    else w->close();
                    closed = true;
                }
                if (!closed) break;
            }
        }
    };

    void next(const std::shared_ptr<Tour> &tour);

    void run(const std::shared_ptr<Tour> &tour) {
        if (tour->at >= tour->steps.size()) {
            if (auto *host = Engine::host()) host->shutdown();
            QCoreApplication::exit(0);
            return;
        }
        const auto &[name, open] = tour->steps[tour->at];
        // Armed first: a step that opens a modal dialog returns only once the dialog is closed.
        QTimer::singleShot(1400, qApp, [tour, name = name] {
            if (!name.isEmpty()) tour->shoot(name);
            tour->clear();
            next(tour);
        });
        open();
    }

    void next(const std::shared_ptr<Tour> &tour) {
        ++tour->at;
        QTimer::singleShot(500, qApp, [tour] { run(tour); });
    }
}

void NewHome::runTour(MainWindow *mw, const QString &dir) {
    auto tour = std::make_shared<Tour>();
    tour->mw = mw;
    QDir().mkpath(dir);
    tour->dir = QDir(dir);
    auto &s = tour->steps;
    const auto action = [](QAction *a) { return [a] { a->trigger(); }; };
    const auto home = [mw] { return mw->newHome; };

    s << std::pair{QString(), std::function<void()>([tour] {
        tour->clear();
        Subscription::updater()->ImportText(QString::fromUtf8(kSampleLinks));
    })};
    for (const QString theme : {QString::fromLatin1(NewStyle::OledTheme), QString::fromLatin1(NewStyle::WhiteTheme), QString::fromLatin1(NewStyle::PastelTheme)}) {
        s << std::pair{QString(), std::function<void()>([tour, mw, theme] {
            tour->theme = theme;
            Configs::dataManager->settingsRepo->theme = theme;
            Configs::dataManager->settingsRepo->new_layout_panel = QStringLiteral("left");
            themeManager()->ApplyTheme(theme);
            mw->showNormal();
            mw->resize(1000, 700);
            mw->move(20, 20);
            mw->raise();
            mw->activateWindow();
        })};
        s << std::pair{QStringLiteral("home"), std::function<void()>([home] {
            if (auto *h = home(); h && h->m_list->model() && h->m_list->model()->rowCount() > 1) h->m_list->setCurrentIndex(h->m_list->model()->index(1, 0));
        })};
        s << std::pair{QStringLiteral("groups"), std::function<void()>([home] { home()->showGroupPicker(); })};
        s << std::pair{QStringLiteral("more-menu"), std::function<void()>([home] { home()->showMoreMenu(); })};
        s << std::pair{QStringLiteral("add-menu"), std::function<void()>([home] { home()->showAddMenu(); })};
        s << std::pair{QStringLiteral("group-menu"), std::function<void()>([home] { home()->showGroupMenu(); })};
        s << std::pair{QStringLiteral("server-menu"), std::function<void()>([mw] { mw->ui->menu_server->popup(QPoint(400, 300)); })};
        s << std::pair{QStringLiteral("tray-menu"), std::function<void()>([mw] { mw->trayMenu->popup(QPoint(600, 300)); })};
        for (int tab = 1; tab <= 4; ++tab) {
            s << std::pair{QStringLiteral("tab%1").arg(tab), std::function<void()>([home, tab] { home()->selectTab(tab, false); })};
        }
        s << std::pair{QStringLiteral("arrange"), std::function<void()>([home] { home()->selectTab(0, false); home()->setArranging(true); })};
        s << std::pair{QStringLiteral("arrange-plain"), std::function<void()>([home] {
            for (auto *w : home()->findChildren<QWidget *>(QStringLiteral("arrangeTutorial"))) w->close();
        })};
        s << std::pair{QStringLiteral("narrow-center"), std::function<void()>([home, mw] {
            home()->setArranging(false);
            Configs::dataManager->settingsRepo->new_layout_panel = QStringLiteral("center");
            home()->applyArrangement();
            mw->resize(380, 760);
        })};
        s << std::pair{QString(), std::function<void()>([home, mw] {
            Configs::dataManager->settingsRepo->new_layout_panel = QStringLiteral("left");
            home()->applyArrangement();
            mw->resize(1000, 700);
        })};
        for (const auto &[name, a] : {std::pair{"add-input", mw->ui->menu_add_from_input}, std::pair{"manage-groups", mw->ui->menu_manage_groups},
                 std::pair{"new-group", mw->ui->actionAdd_New_Group}, std::pair{"edit-group", mw->ui->actionEdit_Group},
                 std::pair{"routing", mw->ui->menu_routing_settings}, std::pair{"vpn", mw->ui->menu_vpn_settings},
                 std::pair{"vk-relay", mw->ui->menu_vk_relay_settings}, std::pair{"presets", mw->ui->menu_preset_settings},
                 std::pair{"integration", mw->ui->menu_integration_settings}, std::pair{"otp", mw->ui->menu_otp_manager},
                 std::pair{"scanner", mw->ui->menu_scanner}, std::pair{"traffic", mw->ui->actionTraffic_Stats},
                 std::pair{"about", mw->ui->actionAbout}, std::pair{"find-settings", mw->ui->menu_find_settings}}) {
            s << std::pair{QString::fromLatin1(name), std::function<void()>(action(a))};
        }
        s << std::pair{QStringLiteral("edit-profile"), std::function<void()>([home, mw] {
            if (auto *h = home(); h->m_list->model() && h->m_list->model()->rowCount() > 0) mw->on_profilesTableView_doubleClicked(h->m_list->model()->index(0, 0));
        })};
        // The settings dialog page by page: it holds most of the program's fields.
        s << std::pair{QStringLiteral("settings"), std::function<void()>(action(mw->ui->menu_basic_settings))};
        for (int page = 1; page < 10; ++page) {
            s << std::pair{QStringLiteral("settings-page%1").arg(page), std::function<void()>([tour, page] {
                QTimer::singleShot(0, qApp, [tour, page] {
                    tour->mw->ui->menu_basic_settings->trigger();
                    QTimer::singleShot(600, qApp, [page] {
                        for (QWidget *w : QApplication::topLevelWidgets()) {
                            if (!w->isVisible() || !qobject_cast<QDialog *>(w)) continue;
                            if (auto *tabs = w->findChild<QTabWidget *>(); tabs && page < tabs->count()) tabs->setCurrentIndex(page);
                        }
                    });
                });
            })};
        }
    }
    QTimer::singleShot(4000, qApp, [tour] { run(tour); });
}
