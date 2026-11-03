// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 rodioniurev
// Standalone main-window verification with an isolated temporary profile.
#include "texstudio.h"
#include "latexeditorview.h"
#include "latexeditorview_config.h"
#include "configmanager.h"
#include "qeditor.h"
#include <QtTest/QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QMenu>
#include <QSettings>
#include <QTextStream>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidgetItemIterator>
#include <QTranslator>
#define CHECK(value) do { if (!(value)) { QTextStream(stderr) << "FAIL line " << __LINE__ << ": " << #value << '\n'; return 1; } } while (false)
int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    CHECK(argc >= 3);
    const bool russian = argc > 4;
    QTranslator translation;
    if (russian) { CHECK(translation.load(QString::fromLocal8Bit(argv[4]))); app.installTranslator(&translation); }
    ConfigManager::configDirOverride = QString::fromLocal8Bit(argv[1]);
    ConfigManager::dontRestoreSession = true;
    Texstudio window;
    window.resize(1000, 700); window.show(); app.processEvents();
    QAction *action = window.findChild<QAction *>("main/options/vimMode");
    CHECK(action && action->isCheckable() && !action->icon().isNull());
    if (russian) CHECK(action->text() == QString::fromUtf8("Режим Vim"));
    auto *options = window.findChild<QMenu *>("main/options");
    CHECK(options && options->actions().contains(action));
    CHECK(QFile::exists(":/images-ng/vim-mode.svg"));
    auto *config = dynamic_cast<ConfigManager *>(ConfigManagerInterface::getInstance());
    CHECK(config);
    auto currentEditor = [&]() {
        LatexEditorView *view = nullptr;
        QMetaObject::invokeMethod(&window, "currentEditorView", Qt::DirectConnection, Q_RETURN_ARG(LatexEditorView *, view));
        return view;
    };
    auto newEditor = [&]() {
        QMetaObject::invokeMethod(&window, "fileNew", Qt::DirectConnection);
        return currentEditor();
    };
    if (argc > 3 && QByteArray(argv[3]) == "--verify-restored") {
        CHECK(!action->isChecked());
        CHECK(!config->editorConfig->vimMode);
        CHECK(newEditor()->editor->property("vimMode").toString().isEmpty());
        bool restoredButton = false;
        for (auto *bar : window.findChildren<QToolBar *>()) if (bar->actions().contains(action)) restoredButton = true;
        CHECK(restoredButton);
        action->trigger(); CHECK(action->isChecked());
        CHECK(currentEditor()->editor->property("vimMode").toString() == "VIM NORMAL");
        QTextStream(stdout) << "PASS: restart restores Vim toggle and customized toolbar action.\n";
    } else {
        CHECK(action->isChecked());
        auto *first = newEditor(); auto *second = newEditor();
        CHECK(first && second && first != second);
        QTest::keyClicks(second->editor, "iPENDING");
        action->trigger();
        CHECK(!action->isChecked() && !config->editorConfig->vimMode);
        CHECK(first->editor->property("vimMode").toString().isEmpty());
        CHECK(second->editor->property("vimMode").toString().isEmpty());
        CHECK(second->editor->text().contains("PENDING"));
        QSettings disk(config->iniPath(), QSettings::IniFormat);
        disk.sync(); CHECK(!disk.value("texmaker/Editor/VimMode", true).toBool());
        CHECK(newEditor()->editor->property("vimMode").toString().isEmpty());
        QToolBar *bar = window.addToolBar("Vim toggle verification"); bar->addAction(action);
        auto *button = qobject_cast<QToolButton *>(bar->widgetForAction(action)); CHECK(button);
        button->click(); CHECK(action->isChecked() && config->editorConfig->vimMode);
        CHECK(first->editor->property("vimMode").toString() == "VIM NORMAL");
        CHECK(second->editor->property("vimMode").toString() == "VIM NORMAL");
        CHECK(newEditor()->editor->property("vimMode").toString() == "VIM NORMAL");
        delete bar;
        bool dialogChecked = false, available = false, added = false;
        QTimer::singleShot(0, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog) return;
            auto *checkbox = dialog->findChild<QCheckBox *>("checkBoxVimMode");
            auto *classes = dialog->findChild<QComboBox *>("comboBoxActions");
            auto *toolbars = dialog->findChild<QComboBox *>("comboBoxToolbars");
            auto *tree = dialog->findChild<QTreeWidget *>("treePossibleToolbarActions");
            if (!checkbox || !classes || !toolbars || !tree) { dialog->reject(); return; }
            dialogChecked = checkbox->isChecked();
            classes->setCurrentIndex(1); toolbars->setCurrentIndex(0);
            for (QTreeWidgetItemIterator it(tree); *it; ++it) {
                if ((*it)->data(0, Qt::UserRole).toString() != action->objectName()) continue;
                available = !(*it)->icon(0).isNull(); tree->setCurrentItem(*it);
                added = QMetaObject::invokeMethod(dialog, "toToolbarClicked", Qt::DirectConnection); break;
            }
            checkbox->setChecked(false); dialog->accept();
        });
        CHECK(QMetaObject::invokeMethod(&window, "generalOptions", Qt::DirectConnection));
        CHECK(dialogChecked && available && added);
        CHECK(!action->isChecked() && !config->editorConfig->vimMode);
        CHECK(first->editor->property("vimMode").toString().isEmpty());
        CHECK(second->editor->property("vimMode").toString().isEmpty());
        bool onToolbar = false;
        for (auto *toolbar : window.findChildren<QToolBar *>()) if (toolbar->actions().contains(action)) onToolbar = true;
        CHECK(onToolbar);
        disk.setValue("texmaker/Editor/VimMode", true); disk.sync(); config->getSettings()->sync();
        CHECK(QMetaObject::invokeMethod(&window, "readSettings", Qt::DirectConnection, Q_ARG(bool, true)));
        CHECK(action->isChecked() && config->editorConfig->vimMode);
        CHECK(first->editor->property("vimMode").toString() == "VIM NORMAL");
        CHECK(second->editor->property("vimMode").toString() == "VIM NORMAL");
        action->trigger(); CHECK(!action->isChecked());
        app.processEvents(); QTest::qWait(50);
        for (auto *toolbar : window.findChildren<QToolBar *>()) if (toolbar->actions().contains(action)) {
            QWidget *widget = toolbar->widgetForAction(action);
            CHECK(widget); widget->grab().save(QString::fromLocal8Bit(argv[2]) + "/vim-toolbar-button.png");
        }
        window.grab().save(QString::fromLocal8Bit(argv[2]) + "/vim-toolbar.png");
        options->popup(window.mapToGlobal(QPoint(20, 45))); app.processEvents();
        options->grab().save(QString::fromLocal8Bit(argv[2]) + (russian ? "/vim-options-menu-ru.png" : "/vim-options-menu.png")); options->hide();
        CHECK(QMetaObject::invokeMethod(&window, "saveSettings", Qt::DirectConnection, Q_ARG(QString, QString())));
        QTextStream(stdout) << "PASS: Options action, icon, all/new tabs, pending input, toolbar customization, checkbox sync and persisted settings.\n";
    }
    for (auto *view : window.findChildren<LatexEditorView *>()) view->editor->document()->setClean();
    return 0;
}
