// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 rodioniurev
// Optional Vim editing for TeXstudio, added and modified 2026-10-24.

#ifndef Header_Vim_Input_Binding
#define Header_Vim_Input_Binding

#include "qeditorinputbinding.h"
#include "qdocumentcursor.h"
#include <QObject>
#include <QHash>
#include <QList>
#include <QStringList>

class QLineEdit;
class QTimer;

// One instance per editor: pending commands and visual anchors belong to a tab.
class VimInputBinding : public QObject, public QEditorInputBinding
{
    Q_OBJECT
public:
    explicit VimInputBinding(QEditor *editor);
    QString id() const override { return QStringLiteral("TXS::VimInputBinding"); }
    QString name() const override { return tr("Vim"); }
    void setEnabled(bool enabled);
    void flushPendingInput() { if (!m_mappingPending.isEmpty()) flushMapping(); }
    bool keyPressEvent(QKeyEvent *event, QEditor *editor) override;
    bool inputMethodEvent(QInputMethodEvent *event, QEditor *editor) override;

signals:
    void modeChanged();
    void escapePressed();
    void applicationCommand(const QString &command, const QString &argument);

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    enum Mode { Normal, Insert, Replace, Visual, VisualLine, VisualBlock, Command };
    struct Key { int key; Qt::KeyboardModifiers modifiers; QString text; };
    struct Register { QString text; int kind = 0; }; // character, line, block
    void setMode(Mode mode);
    void resetCommand();
    void finish(QDocumentCursor cursor);
    void showVisualSelection();
    bool move(QDocumentCursor &cursor, QChar motion, int count, bool explicitCount);
    QDocumentCursor lines(int first, int last) const;
    void operate(QDocumentCursor selection, QChar op, bool linewise);
    void paste(bool before, int count, const Register *value = nullptr);
    bool visual() const;
    int offset(const QDocumentCursor &cursor) const;
    void setOffset(QDocumentCursor &cursor, int position, QDocumentCursor::MoveMode mode = QDocumentCursor::MoveAnchor) const;
    bool inserting() const;
    void recordKey(QKeyEvent *event);
    void completeChange();
    void replay(const QList<Key> &keys, int count);
    void beginInsert();
    void endInsert();
    void updateStatus();
    void message(const QString &text);
    void enterCommand(QChar prompt);
    bool commandKey(QKeyEvent *event);
    void executeEx(QString command);
    bool search(QDocumentCursor &cursor, const QString &pattern, bool backward, int count);
    bool textObject(QDocumentCursor &selection, QChar object, bool around, int count);
    QString bufferText() const;
    bool findCharacter(QDocumentCursor &cursor, QChar kind, QChar target, int count);
    void applyMotion(QDocumentCursor cursor, QChar motion, bool inclusive = false);
    void storeRegister(const QString &text, int kind, QChar op);
    Register readRegister() const;
    void transform(QDocumentCursor selection, QChar op, int count = 1);
    void operateBlock(QChar op);
    void join(int count, bool spaces);
    void addJump();
    void jump(bool backward, int count);
    void changeNumber(int amount);
    struct Definition { QString lhs, rhs; int modes = 0; bool noremap = false; };
    static QList<Definition> s_definitions;
    static QString s_definitionsPath;
    QList<Definition> m_localDefinitions;
    QTimer *m_mappingTimer = nullptr;
    QList<Key> m_mappingPending;
    int m_mappingMode = 0, m_mappingDepth = 0, m_mappingBudget = 0;
    bool m_mappingBypass = false, m_expandingAbbreviation = false, m_literalNext = false;
    bool m_insertOnce = false, m_openRepeat = false;
    QDocumentCursor m_lastInsertPoint;
    QList<QDocumentCursor> m_changes;
    int m_changeIndex = -1, m_sourceDepth = 0;
    int m_shiftWidth = 0, m_abbreviationFloor = 0;
    bool m_abbreviationFresh = true;
    QString m_lastSubstitute, m_lastCommand;
    bool definitionCommand(const QString &name, const QString &argument);
    void loadDefinitions();
    void saveDefinitions();
    int definitionMode() const;
    QList<Key> parseKeys(const QString &text) const;
    QString keyToken(const Key &key) const;
    void dispatchKeys(const QList<Key> &keys, bool remap);
    bool mappingKey(QKeyEvent *event);
    void flushMapping();
    bool expandAbbreviation(QKeyEvent *event);
    bool readCommands(const QString &filename);
    void substitute(const QString &expression, int first, int last);

    QEditor *m_editor;
    bool m_enabled = false;
    bool m_previousOverwrite = false;
    Mode m_mode = Normal;
    int m_count = 0;
    int m_operatorCount = 1;
    QChar m_operator;
    bool m_g = false;
    QDocumentCursor m_visualAnchor, m_visualCursor;
    QDocumentCursor m_lastVisualAnchor, m_lastVisualCursor;
    Mode m_lastVisualMode = Visual;
    QChar m_await, m_objectPrefix, m_register = '"';
    QChar m_findKind, m_findTarget;
    int m_pendingCount = 1;
    bool m_pendingExplicitCount = false;
    static QHash<QChar, Register> m_registers;
    static QHash<QChar, QList<Key>> m_macros;
    QHash<QChar, QDocumentCursor> m_marks;
    QList<QDocumentCursor> m_jumps, m_forwardJumps;
    QList<Key> m_candidate, m_lastChange;
    QString m_beforeChange;
    QString m_insertBase;
    QList<Key> m_insertSequence;
    int m_insertRepeat = 1;
    bool m_ignoreChange = false, m_insertBlock = false, m_replaying = false;
    int m_replayDepth = 0, m_replayBudget = 0;
    QChar m_recording, m_lastMacro;
    QList<Key> m_recorded;
    QLineEdit *m_commandLine = nullptr;
    QString m_commandText, m_searchPattern;
    QChar m_prompt;
    bool m_searchBackward = false, m_ignoreCase = false, m_smartCase = false;
    int m_commandPosition = 0, m_historyPosition = 0;
    QHash<QChar, QStringList> m_history;
    int m_rangeFirst = -1, m_rangeLast = -1;
    QChar m_searchOperator;
    int m_searchCount = 1;
    Mode m_commandPreviousMode = Normal;
};

#endif
