// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 rodioniurev
// Optional Vim editing for TeXstudio, added and modified 2026-10-24.

#ifndef Header_Vim_Input_Binding_Test
#define Header_Vim_Input_Binding_Test
#ifndef QT_NO_DEBUG
#include <QObject>
#include <memory>
class QEditor;
class VimInputBinding;
class QMimeData;

class VimInputBindingTest : public QObject
{
    Q_OBJECT
public:
    VimInputBindingTest();
    ~VimInputBindingTest();
private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    void modesAndDisabledInput();
    void motions();
    void operators_data();
    void operators();
    void linewiseClipboard();
    void visualSelection();
    void undoAndRedo();
    void readOnly();
    void perEditorState();
    void shortcutsAndInputMethod();
    void objects_data();
    void objects();
    void objectOracle_data();
    void objectOracle();
    void objectWorkflow();
    void searching();
    void exCommands_data();
    void exCommands();
    void commandHistoryAndErrors();
    void repeatAndMacros();
    void namedRegisters();
    void characterFind();
    void marksAndJumps();
    void extraEditing_data();
    void extraEditing();
    void replaceAndNumbers();
    void blockEditing();
    void extendedReadOnly();
    void applicationCommands();
    void insertControlsAndUndo();
private:
    void keys(const char *text);
    void ex(const QString &text);
    void escape();
    std::unique_ptr<QEditor> editor;
    VimInputBinding *vim = nullptr;
    std::unique_ptr<QMimeData> clipboard;
    int tabStop = 4;
};
#endif
#endif
