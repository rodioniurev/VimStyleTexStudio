// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 rodioniurev
// Optional Vim editing for TeXstudio, added and modified 2026-10-24.

#include "viminputbinding.h"
#include "qeditor.h"
#include "qdocument.h"
#include "qdocumentline.h"
#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QMimeData>
#include <QLineEdit>
#include <QInputMethodEvent>
#include <QScrollBar>
#include <QRegularExpression>
#include <QTimer>
#include <QScopeGuard>

namespace {
constexpr int maxCount = 10000;

int wordClass(QChar ch)
{
    if (ch.isNull() || ch.isSpace()) return 0;
    return ch.isLetterOrNumber() || ch == '_' ? 1 : 2;
}

void clampNormal(QDocumentCursor &cursor)
{
    cursor.clearSelection();
    cursor.setColumnNumber(qMin(cursor.columnNumber(), qMax(0, cursor.line().length() - 1)));
}
}

VimInputBinding::VimInputBinding(QEditor *editor) : QObject(editor), m_editor(editor)
{
    editor->installEventFilter(this);
    loadDefinitions();
    m_mappingTimer = new QTimer(this);
    m_mappingTimer->setSingleShot(true);
    m_mappingTimer->setInterval(1000);
    connect(m_mappingTimer, &QTimer::timeout, this, &VimInputBinding::flushMapping);
    m_commandLine = new QLineEdit(editor->viewport());
    m_commandLine->setObjectName("vimCommandLine");
    m_commandLine->setFocusPolicy(Qt::NoFocus);
    m_commandLine->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_commandLine->hide();
}

void VimInputBinding::resetCommand()
{
    completeChange();
    m_count = 0;
    m_operatorCount = 1;
    m_operator = QChar();
    m_g = false;
    m_await = QChar();
    m_objectPrefix = QChar();
    m_register = '"';
}

void VimInputBinding::setEnabled(bool enabled)
{
    if (m_enabled == enabled) return;
    if (!enabled) { flushPendingInput(); m_mappingTimer->stop(); }
    m_enabled = enabled;
    resetCommand();
    if (enabled) {
        emit escapePressed();
        m_previousOverwrite = m_editor->flag(QEditor::Overwrite);
        m_editor->setFlag(QEditor::Overwrite, false);
        setMode(Normal);
    } else {
        endInsert();
        m_mode = Normal;
        m_commandLine->hide();
        m_recording = QChar();
        m_candidate.clear();
        m_editor->setProperty("vimMode", QString());
        m_editor->setFlag(QEditor::Overwrite, m_previousOverwrite);
        QDocumentCursor cursor = m_editor->cursor();
        cursor.clearSelection();
        m_editor->setCursor(cursor);
        emit modeChanged();
    }
}

void VimInputBinding::setMode(Mode mode)
{
    if (mode == Insert || mode == Replace) m_insertOnce = false;
    if (visual() && mode != m_mode) {
        m_lastVisualAnchor = m_visualAnchor;
        m_lastVisualCursor = m_visualCursor;
        m_lastVisualMode = m_mode;
        m_lastVisualAnchor.setAutoUpdated(true); m_lastVisualCursor.setAutoUpdated(true);
    }
    if ((mode == Insert || mode == Replace) && !inserting()) {
        beginInsert();
        m_insertBase = bufferText();
        m_insertSequence.clear();
        m_insertRepeat = 1; m_openRepeat = false; m_abbreviationFresh = true;
    }
    else if (mode != Insert && mode != Replace) endInsert();
    m_mode = mode;
    m_editor->setFlag(QEditor::Overwrite, mode == Replace);
    resetCommand();
    if (mode == Normal) {
        m_editor->clearPlaceHolders();
        finish(m_editor->cursor());
    }
    updateStatus();
}

bool VimInputBinding::eventFilter(QObject *object, QEvent *event)
{
    if (object == m_editor && m_enabled) {
        if (event->type() == QEvent::KeyPress) {
            auto *key = static_cast<QKeyEvent *>(event);
            if (m_mode != Command) m_commandLine->hide();
            if (inserting()) {
                if (m_abbreviationFresh) { m_abbreviationFloor = offset(m_editor->cursor()); m_abbreviationFresh = false; }
                m_abbreviationFloor = qMin(m_abbreviationFloor, offset(m_editor->cursor()));
            }
            if (mappingKey(key)) return true;
            recordKey(key);
            if (expandAbbreviation(key)) return true;
            if (m_expandingAbbreviation && inserting() && m_await.isNull() && !key->text().isEmpty()
                && !(key->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))
                && key->key() != Qt::Key_Return && key->key() != Qt::Key_Enter && key->key() != Qt::Key_Tab) {
                if (!m_editor->isReadOnly()) m_editor->write(key->text());
                return true;
            }
            if (m_literalNext && (inserting() || m_mode == Command)) {
                m_literalNext = false;
                if (m_mode == Command) {
                    m_commandText.insert(m_commandPosition, key->text()); m_commandPosition += key->text().size();
                    message(QString(m_prompt) + m_commandText);
                } else if (!m_editor->isReadOnly()) m_editor->write(key->text());
                return true;
            }
            if (key->key() == Qt::Key_Escape || (key->modifiers() == Qt::ControlModifier
                && (key->key() == Qt::Key_BracketLeft || key->key() == Qt::Key_C
                    || (inserting() && (QStringLiteral("WURTDEYAOV").contains(QChar(key->key())) || key->key() == Qt::Key_BracketRight))))) {
                emit escapePressed(); // Close a temporary completion binding before changing modes.
                return keyPressEvent(key, m_editor);
            }
        } else if (event->type() == QEvent::ShortcutOverride) {
            auto *key = static_cast<QKeyEvent *>(event);
            const bool escape = key->key() == Qt::Key_Escape
                || (key->modifiers() == Qt::ControlModifier && (key->key() == Qt::Key_BracketLeft || key->key() == Qt::Key_C));
            const bool plain = !(key->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
            const bool vimKey = !key->text().isEmpty() || key->key() == Qt::Key_Left || key->key() == Qt::Key_Right
                || key->key() == Qt::Key_Up || key->key() == Qt::Key_Down || key->key() == Qt::Key_Home
                || key->key() == Qt::Key_End || key->key() == Qt::Key_PageUp || key->key() == Qt::Key_PageDown
                || key->key() == Qt::Key_Delete || key->key() == Qt::Key_Backspace || key->key() == Qt::Key_Tab
                || key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter;
            const bool hasMapping = !m_mappingPending.isEmpty() || [&] {
                const QString token = keyToken({key->key(), key->modifiers(), key->text()});
                for (const auto &entry : s_definitions + m_localDefinitions)
                    if (!entry.lhs.startsWith("abbr:") && (entry.modes & definitionMode()) && entry.lhs.startsWith(token)) return true;
                return false;
            }();
            const bool vimControl = key->modifiers() == Qt::ControlModifier
                && QStringLiteral("RDUFBEYAOIVXC").contains(QChar(key->key()));
            if (hasMapping || escape || m_mode == Command || (!inserting() && ((plain && vimKey) || vimControl))
                || (inserting() && key->modifiers() == Qt::ControlModifier
                    && (QStringLiteral("WURTDEYAOV").contains(QChar(key->key())) || key->key() == Qt::Key_BracketRight))) {
                event->accept();
                return true;
            }
        } else if (event->type() == QEvent::Resize && !m_commandLine->isHidden()) {
            message(m_commandLine->text());
        } else if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::FocusOut) {
            flushPendingInput(); m_literalNext = false;
            resetCommand();
            if (visual() || m_mode == Command) { m_commandLine->hide(); setMode(Normal); }
        }
    }
    return QObject::eventFilter(object, event);
}

bool VimInputBinding::inputMethodEvent(QInputMethodEvent *event, QEditor *)
{
    if (!m_enabled) return false;
    if (!event->commitString().isEmpty() && inserting() && !m_replaying) {
        if (m_candidate.isEmpty()) m_beforeChange = bufferText();
        const Key key{0, Qt::NoModifier, event->commitString()};
        m_candidate.append(key);
        m_insertSequence.append(key);
        if (!m_recording.isNull()) m_recorded.append(key);
    }
    if (m_mode == Command) {
        m_commandText.insert(m_commandPosition, event->commitString());
        m_commandPosition += event->commitString().size();
        message(QString(m_prompt) + m_commandText);
        return true;
    }
    return !inserting();
}

void VimInputBinding::finish(QDocumentCursor cursor)
{
    if (m_mode == Normal) clampNormal(cursor);
    m_editor->setCursor(cursor);
    m_editor->ensureCursorVisible();
}

QDocumentCursor VimInputBinding::lines(int first, int last) const
{
    QDocument *doc = m_editor->document();
    first = qBound(0, first, doc->lines() - 1);
    last = qBound(first, last, doc->lines() - 1);
    QDocumentCursor cursor(doc, first, 0);
    if (last + 1 < doc->lines()) cursor.moveTo(last + 1, 0, QDocumentCursor::KeepAnchor);
    else cursor.movePosition(1, QDocumentCursor::End, QDocumentCursor::KeepAnchor);
    return cursor;
}

void VimInputBinding::showVisualSelection()
{
    QDocumentCursor first = m_visualAnchor, last = m_visualCursor;
    if (last < first) qSwap(first, last);
    if (m_mode == VisualBlock) {
        const int top = qMin(first.lineNumber(), last.lineNumber()), bottom = qMax(first.lineNumber(), last.lineNumber());
        const int left = qMin(m_visualAnchor.columnNumber(), m_visualCursor.columnNumber());
        const int right = qMax(m_visualAnchor.columnNumber(), m_visualCursor.columnNumber()) + 1;
        for (int line = top; line <= bottom; ++line) {
            QDocumentCursor block(m_editor->document(), line, qMin(left, m_editor->document()->line(line).length()));
            block.moveTo(line, qMin(right, block.line().length()), QDocumentCursor::KeepAnchor);
            if (line == top) m_editor->setCursor(block);
            else m_editor->addCursorMirror(block);
        }
    } else if (m_mode == VisualLine) {
        m_editor->setCursor(lines(first.lineNumber(), last.lineNumber()));
    } else {
        if (!last.atLineEnd()) last.movePosition(1, QDocumentCursor::NextCharacter);
        first.moveTo(last, QDocumentCursor::KeepAnchor);
        m_editor->setCursor(first);
    }
    m_editor->ensureCursorVisible();
}

bool VimInputBinding::move(QDocumentCursor &c, QChar motion, int count, bool explicitCount)
{
    using C = QDocumentCursor;
    if (motion == 'h') c.setColumnNumber(qMax(0, c.columnNumber() - count));
    else if (motion == 'l') c.setColumnNumber(qMin(c.line().length(), c.columnNumber() + count));
    else if (motion == 'j' || motion == 'k') c.movePosition(count, motion == 'j' ? C::Down : C::Up);
    else if (motion == '0') c.movePosition(1, C::StartOfLine);
    else if (motion == '^') c.movePosition(1, C::StartOfLineText);
    else if (motion == '$') {
        c.movePosition(count - 1, C::Down);
        c.movePosition(1, C::EndOfLine);
    } else if (motion == 'G' || motion == 'g') {
        const int line = explicitCount ? qMin(count, m_editor->document()->lines()) - 1
                                     : motion == 'g' ? 0 : m_editor->document()->lines() - 1;
        c.moveTo(line, 0);
        c.movePosition(1, C::StartOfLineText);
    } else if (motion == '|' ) c.setColumnNumber(qMin(c.line().length(), count - 1));
    else if (motion == '+' || motion == '-') {
        c.movePosition(count, motion == '+' ? C::Down : C::Up); c.movePosition(1, C::StartOfLineText);
    } else if (motion == '{' || motion == '}') {
        const int direction = motion == '}' ? 1 : -1;
        int line = c.lineNumber();
        for (int n = 0; n < count; ++n) {
            line = qBound(0, line + direction, m_editor->document()->lines() - 1);
            while (line > 0 && line + 1 < m_editor->document()->lines() && m_editor->document()->line(line).text().trimmed().isEmpty()) line += direction;
            while (line > 0 && line + 1 < m_editor->document()->lines() && !m_editor->document()->line(line).text().trimmed().isEmpty()) line += direction;
        }
        c.moveTo(line, 0);
    } else if (motion == '(' || motion == ')') {
        const QString text = bufferText();
        int position = offset(c), direction = motion == ')' ? 1 : -1;
        for (int n = 0; n < count; ++n) {
            position += direction;
            while (position > 0 && position < text.size() && !(position >= 2 && text.at(position - 1).isSpace() && QStringLiteral(".!?").contains(text.at(position - 2)))) position += direction;
        }
        setOffset(c, qBound(0, position, int(text.size())));
    } else if (motion == 'H' || motion == 'M' || motion == 'L') {
        const int visible = qMax(1, int(m_editor->viewport()->height() / QDocument::getLineSpacing()));
        const int top = m_editor->verticalScrollBar()->value();
        const int line = motion == 'H' ? top + count - 1 : motion == 'M' ? top + visible / 2 : top + visible - count;
        c.moveTo(qBound(0, line, m_editor->document()->lines() - 1), 0); c.movePosition(1, C::StartOfLineText);
    } else if (motion == '%') {
        if (explicitCount) { c.moveTo(qBound(0, (m_editor->document()->lines() * qMin(100, count) + 99) / 100 - 1, m_editor->document()->lines() - 1), 0); return true; }
        const QString text = bufferText(), brackets("()[]{}");
        int start = offset(c);
        while (start < text.size() && text.at(start) != '\n' && !brackets.contains(text.at(start))) ++start;
        if (start >= text.size() || !brackets.contains(text.at(start))) return true;
        const int index = brackets.indexOf(text.at(start)), direction = index % 2 ? -1 : 1;
        const QChar same = text.at(start), other = brackets.at(index + direction);
        int depth = 1;
        for (int i = start + direction; i >= 0 && i < text.size(); i += direction) {
            if (text.at(i) == same) ++depth;
            if (text.at(i) == other && !--depth) { setOffset(c, i); break; }
        }
    } else if (QStringLiteral("wbeWBE").contains(motion) && !motion.isNull()) {
        const bool big = motion.isUpper();
        motion = motion.toLower();
        auto typeOf = [big](QChar ch) { return big && !ch.isSpace() && !ch.isNull() ? 1 : wordClass(ch); };
        for (int n = 0; n < count; ++n) {
            if (motion == 'w') {
                const int type = typeOf(c.nextChar());
                while (!c.atEnd() && typeOf(c.nextChar()) == type)
                    c.movePosition(1, C::NextCharacter);
                while (!c.atEnd() && typeOf(c.nextChar()) == 0)
                    c.movePosition(1, C::NextCharacter);
            } else if (motion == 'b') {
                if (!c.atStart()) c.movePosition(1, C::PreviousCharacter);
                while (!c.atStart() && typeOf(c.nextChar()) == 0)
                    c.movePosition(1, C::PreviousCharacter);
                const int type = typeOf(c.nextChar());
                while (!c.atStart() && typeOf(c.previousChar()) == type)
                    c.movePosition(1, C::PreviousCharacter);
            } else {
                if (!c.atEnd()) c.movePosition(1, C::NextCharacter);
                while (!c.atEnd() && typeOf(c.nextChar()) == 0)
                    c.movePosition(1, C::NextCharacter);
                const int type = typeOf(c.nextChar());
                while (!c.atEnd() && typeOf(c.nextChar()) == type) {
                    QDocumentCursor next = c;
                    next.movePosition(1, C::NextCharacter);
                    if (next.atEnd() || typeOf(next.nextChar()) != type) break;
                    c = next;
                }
            }
        }
    } else return false;
    return true;
}

void VimInputBinding::operate(QDocumentCursor selection, QChar op, bool linewise)
{
    if (op == '>' || op == '<' || op == 'u' || op == 'U' || op == '~') { transform(selection, op); return; }
    if (op != 'y' && m_editor->isReadOnly()) return;
    QString text = selection.selectedText();
    if (linewise && !text.endsWith('\n')) text += '\n';
    storeRegister(text, linewise ? 1 : 0, op);
    if (op == 'c') beginInsert();
    QDocumentCursor start = selection.selectionStart();
    if (op != 'y') {
        // The last line has no following newline; remove its preceding separator.
        if (linewise && op == 'd' && selection.selectionEnd().atEnd() && start.lineNumber() > 0) {
            QDocumentCursor end = selection.selectionEnd();
            start.movePosition(1, QDocumentCursor::PreviousCharacter);
            selection = start;
            selection.moveTo(end, QDocumentCursor::KeepAnchor);
        }
        selection.beginEditBlock();
        selection.removeSelectedText();
        if (linewise && op == 'c' && !selection.atEnd()) {
            selection.insertText("\n");
            selection.movePosition(1, QDocumentCursor::PreviousCharacter);
        }
        selection.endEditBlock();
        start = selection;
    }
    setMode(op == 'c' ? Insert : Normal);
    finish(start);
}

void VimInputBinding::paste(bool before, int count, const Register *value)
{
    if (m_editor->isReadOnly()) return;
    const Register reg = value ? *value : readRegister();
    if (reg.text.isEmpty()) return;
    QDocumentCursor c = m_editor->cursor();
    c.clearSelection();
    const bool linewise = reg.kind == 1;
    QString text = reg.text.repeated(count);
    c.beginEditBlock();
    if (reg.kind == 2) {
        const QStringList pieces = reg.text.split('\n');
        const int top = c.lineNumber(), column = c.columnNumber() + (before || c.atLineEnd() ? 0 : 1);
        for (int i = 0; i < pieces.size(); ++i) {
            if (top + i >= m_editor->document()->lines()) {
                QDocumentCursor end(m_editor->document()); end.movePosition(1, QDocumentCursor::End); end.insertText("\n");
            }
            QDocumentCursor row(m_editor->document(), top + i, qMin(column, m_editor->document()->line(top + i).length()));
            if (row.columnNumber() < column) row.insertText(QString(column - row.columnNumber(), ' '));
            row.insertText(pieces.at(i).repeated(count));
        }
        c.moveTo(top, column);
    } else if (linewise) {
        if (before) c.movePosition(1, QDocumentCursor::StartOfLine);
        else if (c.lineNumber() + 1 < m_editor->document()->lines()) c.moveTo(c.lineNumber() + 1, 0);
        else {
            c.movePosition(1, QDocumentCursor::EndOfLine);
            c.insertText("\n");
            text.chop(1);
        }
        const int line = c.lineNumber();
        c.insertText(text);
        c.moveTo(line, 0);
        c.movePosition(1, QDocumentCursor::StartOfLineText);
    } else {
        if (!before && !c.atLineEnd()) c.movePosition(1, QDocumentCursor::NextCharacter);
        c.insertText(text);
        c.movePosition(1, QDocumentCursor::PreviousCharacter);
    }
    c.endEditBlock();
    finish(c);
}

bool VimInputBinding::keyPressEvent(QKeyEvent *event, QEditor *)
{
    if (!m_enabled) return false;
    const bool resumeInsert = m_insertOnce;
    const auto returnToInsert = qScopeGuard([this, resumeInsert] {
        if (resumeInsert && m_insertOnce && m_mode == Normal && !m_count && m_operator.isNull() && !m_g && m_await.isNull()) {
            m_insertOnce = false; setMode(Insert);
        }
    });
    const bool control = event->modifiers() == Qt::ControlModifier;
    if (event->key() == Qt::Key_Escape || (control && (event->key() == Qt::Key_BracketLeft || event->key() == Qt::Key_C))) {
        if (inserting() && m_insertRepeat > 1) {
            const int repetitions = m_insertRepeat - 1; m_insertRepeat = 1;
            QList<Key> sequence = m_insertSequence;
            if (m_openRepeat) sequence.prepend({Qt::Key_Return, Qt::NoModifier, QString("\r")});
            replay(sequence, repetitions);
        }
        QDocumentCursor c = visual() ? m_visualCursor : m_editor->cursor();
        if (inserting() && c.columnNumber() > 0) c.movePosition(1, QDocumentCursor::PreviousCharacter);
        m_commandLine->hide();
        setMode(Normal);
        finish(c);
        return true;
    }
    if (m_mode == Command) return commandKey(event);
    if (inserting()) {
        if (m_await == 'r') {
            m_register = event->text().isEmpty() ? QChar('"') : event->text().at(0);
            if (!m_editor->isReadOnly()) m_editor->write(readRegister().text);
            m_await = QChar(); m_register = '"'; return true;
        }
        if (control && event->key() == Qt::Key_V) { m_literalNext = true; return true; }
        if (control && event->key() == Qt::Key_BracketRight) return true;
        if (control && event->key() == Qt::Key_O) {
            endInsert(); m_mode = Normal; m_insertOnce = true;
            resetCommand(); updateStatus(); return true;
        }
        if (control && (event->key() == Qt::Key_A || event->key() == Qt::Key_At)) {
            if (!m_editor->isReadOnly()) m_editor->write(m_registers.value('.').text);
            if (event->key() == Qt::Key_At) setMode(Normal);
            return true;
        }
        if (control && (event->key() == Qt::Key_E || event->key() == Qt::Key_Y)) {
            QDocumentCursor c = m_editor->cursor();
            const int row = c.lineNumber() + (event->key() == Qt::Key_E ? 1 : -1);
            if (!m_editor->isReadOnly() && row >= 0 && row < m_editor->document()->lines()) {
                const QString adjacent = m_editor->document()->line(row).text();
                if (c.columnNumber() < adjacent.size()) m_editor->write(adjacent.mid(c.columnNumber(), 1));
            }
            return true;
        }
        if (control && (event->key() == Qt::Key_T || event->key() == Qt::Key_D)) {
            if (!m_editor->isReadOnly()) {
                QDocumentCursor c = m_editor->cursor(), change(m_editor->document(), c.lineNumber(), 0);
                const int width = m_shiftWidth ? m_shiftWidth : QDocument::tabStop();
                if (event->key() == Qt::Key_T) change.insertText(QString(width, ' '));
                else {
                    int amount = 0; const QString text = c.line().text();
                    while (amount < text.size() && amount < width && text.at(amount).isSpace()) ++amount;
                    change.movePosition(amount, QDocumentCursor::NextCharacter, QDocumentCursor::KeepAnchor);
                    change.removeSelectedText();
                }
                finish(c);
            }
            return true;
        }
        if (control && event->key() == Qt::Key_R) { m_await = 'r'; return true; }
        if (control && (event->key() == Qt::Key_W || event->key() == Qt::Key_U)) {
            if (m_editor->isReadOnly()) return true;
            QDocumentCursor c = m_editor->cursor(), start = c;
            start.clearSelection();
            if (event->key() == Qt::Key_W) move(start, 'b', 1, false);
            else start.movePosition(1, QDocumentCursor::StartOfLine);
            c.moveTo(start, QDocumentCursor::KeepAnchor);
            c.removeSelectedText(); finish(c); return true;
        }
        if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right || event->key() == Qt::Key_Up || event->key() == Qt::Key_Down
            || event->key() == Qt::Key_Home || event->key() == Qt::Key_End) m_abbreviationFresh = true;
        return false;
    }
    if (event->key() == Qt::Key_Shift || event->key() == Qt::Key_Control || event->key() == Qt::Key_Alt || event->key() == Qt::Key_Meta) return true;
    const int count = qMin(maxCount, qMax(1, m_count) * m_operatorCount);
    const bool explicitCount = m_count != 0 || m_operatorCount != 1;
    QDocumentCursor cursor = visual() ? m_visualCursor : m_editor->cursor();
    cursor.clearSelection();
    if (m_mode == Normal) clampNormal(cursor);
    if (control) {
        if (event->key() == Qt::Key_V) {
            if (m_mode == VisualBlock) setMode(Normal);
            else { m_visualAnchor = cursor; m_visualCursor = cursor; setMode(VisualBlock); showVisualSelection(); }
        } else if (event->key() == Qt::Key_R) {
            m_ignoreChange = true;
            if (!m_editor->isReadOnly()) for (int i = 0; i < count; ++i) m_editor->redo();
            finish(m_editor->cursor());
        } else if (event->key() == Qt::Key_O || event->key() == Qt::Key_I) jump(event->key() == Qt::Key_O, count);
        else if (event->key() == Qt::Key_A || event->key() == Qt::Key_X) changeNumber(event->key() == Qt::Key_A ? count : -count);
        else if (QStringLiteral("DUFB").contains(QChar(event->key()))) {
            const int height = qMax(1, int(m_editor->viewport()->height() / QDocument::getLineSpacing()));
            const bool up = event->key() == Qt::Key_U || event->key() == Qt::Key_B;
            const int distance = (event->key() == Qt::Key_D || event->key() == Qt::Key_U ? qMax(1, height / 2) : height) * count;
            cursor.movePosition(distance, up ? QDocumentCursor::Up : QDocumentCursor::Down);
            applyMotion(cursor, up ? 'k' : 'j');
        } else if (event->key() == Qt::Key_E || event->key() == Qt::Key_Y) {
            auto *bar = m_editor->verticalScrollBar();
            bar->setValue(bar->value() + (event->key() == Qt::Key_E ? count : -count));
        } else { resetCommand(); return false; }
        resetCommand(); return true;
    }
    if (event->modifiers() & (Qt::AltModifier | Qt::MetaModifier)) { resetCommand(); return false; }
    QChar key = event->text().size() == 1 ? event->text().at(0) : QChar();
    if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Backspace) key = 'h';
    if (event->key() == Qt::Key_Right || event->key() == Qt::Key_Space) key = 'l';
    if (event->key() == Qt::Key_Up) key = 'k';
    if (event->key() == Qt::Key_Down) key = 'j';
    if (event->key() == Qt::Key_Home) key = '0';
    if (event->key() == Qt::Key_End) key = '$';
    if (event->key() == Qt::Key_Delete) key = 'x';
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) key = '+';
    if (event->key() == Qt::Key_PageDown || event->key() == Qt::Key_PageUp) {
        cursor.movePosition(count * qMax(1, int(m_editor->viewport()->height() / QDocument::getLineSpacing())),
                            event->key() == Qt::Key_PageDown ? QDocumentCursor::Down : QDocumentCursor::Up);
        applyMotion(cursor, 'j'); resetCommand(); return true;
    }
    if (event->key() == Qt::Key_Tab) { jump(false, count); resetCommand(); return true; }

    if (!m_await.isNull()) {
        const QChar awaiting = m_await; m_await = QChar();
        if (awaiting == '"') {
            if (key.isLetterOrNumber() || QStringLiteral("\"+*_-./:%").contains(key)) m_register = key;
            else resetCommand();
            return true;
        }
        if (awaiting == 'q') {
            if (key.isLetter()) { m_recording = key.toLower(); m_recorded = key.isUpper() ? m_macros.value(key.toLower()) : QList<Key>(); }
            resetCommand(); updateStatus(); return true;
        }
        if (awaiting == '@') {
            const QChar target = key == '@' ? m_lastMacro : key.toLower();
            m_lastMacro = target;
            resetCommand(); replay(m_macros.value(target), m_pendingCount); return true;
        }
        if (awaiting == 'm') {
            if (key.isLetter()) { cursor.setAutoUpdated(true); m_marks[key] = cursor; }
        } else if (awaiting == '\'' || awaiting == '`') {
            QDocumentCursor mark = key == awaiting && !m_jumps.isEmpty() ? m_jumps.last() : m_marks.value(key);
            if (mark.isValid()) {
                addJump();
                if (awaiting == '\'') mark.movePosition(1, QDocumentCursor::StartOfLineText);
                applyMotion(mark, awaiting);
            } else message(tr("Mark not set"));
        } else if (awaiting == 'r') {
            if (!m_editor->isReadOnly() && !key.isNull()) {
                if (visual()) {
                    if (m_mode == VisualBlock) {
                        const int top = qMin(m_visualAnchor.lineNumber(), m_visualCursor.lineNumber()), bottom = qMax(m_visualAnchor.lineNumber(), m_visualCursor.lineNumber());
                        const int left = qMin(m_visualAnchor.columnNumber(), m_visualCursor.columnNumber()), right = qMax(m_visualAnchor.columnNumber(), m_visualCursor.columnNumber()) + 1;
                        cursor.beginEditBlock();
                        for (int line = bottom; line >= top; --line) {
                            QDocumentCursor part(m_editor->document(), line, qMin(left, m_editor->document()->line(line).length()));
                            part.moveTo(line, qMin(right, part.line().length()), QDocumentCursor::KeepAnchor);
                            part.insertText(QString(part.selectedText().size(), key));
                        }
                        cursor.endEditBlock();
                    } else {
                        QDocumentCursor selection = m_editor->cursor(); QString text = selection.selectedText();
                        for (int i = 0; i < text.size(); ++i) if (text.at(i) != '\n') text[i] = key;
                        selection.beginEditBlock(); selection.insertText(text); selection.endEditBlock();
                    }
                    setMode(Normal); finish(cursor);
                } else if (cursor.columnNumber() + m_pendingCount <= cursor.line().length()) {
                    QDocumentCursor start = cursor;
                    cursor.movePosition(m_pendingCount, QDocumentCursor::NextCharacter, QDocumentCursor::KeepAnchor);
                    cursor.beginEditBlock(); cursor.insertText(QString(m_pendingCount, key)); cursor.endEditBlock();
                    start.movePosition(m_pendingCount - 1, QDocumentCursor::NextCharacter); finish(start);
                }
            }
        } else if (awaiting == 'i' || awaiting == 'a') {
            QDocumentCursor object = cursor;
            if (visual() && m_visualAnchor != m_visualCursor && QStringLiteral("()b{}B[]<>").contains(key))
                object = m_editor->cursor().selectionStart();
            if (textObject(object, key, awaiting == 'a', m_pendingCount)) {
                if (!m_operator.isNull()) {
                    bool linewise = key == 'p';
                    const QDocumentCursor first = object.selectionStart(), last = object.selectionEnd();
                    if (first.lineNumber() != last.lineNumber()) {
                        if (first.columnNumber() == 0 && last.columnNumber() == 0) linewise = true;
                        else if (m_operator == 'd' && first.line().text().left(first.columnNumber()).trimmed().isEmpty()
                                 && last.line().text().mid(last.columnNumber()).trimmed().isEmpty()) {
                            object = lines(first.lineNumber(), last.lineNumber()); linewise = true;
                        }
                    }
                    operate(object, m_operator, linewise);
                }
                else {
                    if (m_mode == Visual || m_mode == VisualLine) {
                        QDocumentCursor previous = m_editor->cursor();
                        if (m_mode == VisualLine && m_visualAnchor == m_visualCursor) {
                            previous = m_visualAnchor;
                            previous.movePosition(1, QDocumentCursor::NextCharacter, QDocumentCursor::KeepAnchor);
                        }
                        const bool forward = m_visualCursor >= m_visualAnchor;
                        if (QStringLiteral("wWsp").contains(key) && m_visualAnchor != m_visualCursor) {
                            if (forward) {
                                if (object.selectionEnd() <= previous.selectionEnd() || (awaiting == 'a' && m_visualAnchor != m_visualCursor)) {
                                    object = previous.selectionEnd();
                                    textObject(object, key, awaiting == 'a', m_pendingCount);
                                }
                                QDocumentCursor merged = previous.selectionStart();
                                merged.moveTo(object.selectionEnd(), QDocumentCursor::KeepAnchor); object = merged;
                            } else {
                                if (object.selectionStart() >= previous.selectionStart()) {
                                    object = previous.selectionStart();
                                    object.movePosition(1, QDocumentCursor::PreviousCharacter);
                                    textObject(object, key, awaiting == 'a', m_pendingCount);
                                }
                                QDocumentCursor merged = previous.selectionEnd();
                                merged.moveTo(object.selectionStart(), QDocumentCursor::KeepAnchor); object = merged;
                            }
                        } else if ((awaiting == 'i' || key == 't') && QStringLiteral("()b{}B[]<>t").contains(key)
                                   && object.selectionStart() >= previous.selectionStart()
                                   && object.selectionEnd() <= previous.selectionEnd()) {
                            QDocumentCursor outer = cursor;
                            if (key == 't' && awaiting == 'i' && textObject(outer, key, true, m_pendingCount)) object = outer;
                            else if (key == 't' && textObject(outer, key, true, m_pendingCount + 1)) object = outer;
                            else if (textObject(outer, key, false, m_pendingCount + 1)) object = outer;
                            else object = previous;
                        }
                    }
                    m_mode = key == 'p' ? VisualLine : Visual;
                    m_visualAnchor = object.selectionStart(); m_visualCursor = object.selectionEnd();
                    if (m_visualCursor > m_visualAnchor) m_visualCursor.movePosition(1, QDocumentCursor::PreviousCharacter);
                    showVisualSelection();
                    updateStatus();
                }
            } else message(tr("Text object not found"));
        } else if (QStringLiteral("fFtT").contains(awaiting)) {
            if (findCharacter(cursor, awaiting, key, m_pendingCount)) {
                m_findKind = awaiting; m_findTarget = key;
                applyMotion(cursor, awaiting, true);
            }
        } else if (awaiting == 'z') {
            auto *bar = m_editor->verticalScrollBar();
            const int height = qMax(1, int(m_editor->viewport()->height() / QDocument::getLineSpacing()));
            if (key == 'z' || key == 't' || key == 'b') bar->setValue(qMax(0, cursor.lineNumber() - (key == 'z' ? height / 2 : key == 'b' ? height - 1 : 0)));
        } else if (awaiting == 'Z') {
            if (key == 'Z' || key == 'Q') {
                const QString cmd = key == 'Z' ? "x" : "q!";
                QMetaObject::invokeMethod(this, [this, cmd] { emit applicationCommand(cmd, QString()); }, Qt::QueuedConnection);
            }
        }
        resetCommand(); return true;
    }
    if (key >= '0' && key <= '9' && (key != '0' || m_count)) {
        m_count = qMin(maxCount, m_count * 10 + key.digitValue()); return true;
    }
    if (m_g) {
        m_g = false;
        if (key == 'u' || key == 'U' || key == '~') {
            m_operator = key; m_operatorCount = count; m_count = 0;
            if (visual()) { if (m_mode == VisualBlock) operateBlock(key); else transform(m_editor->cursor(), key); resetCommand(); }
            return true;
        }
        if (key == 'J') { join(count, false); resetCommand(); return true; }
        if (key == 'i') {
            if (!m_editor->isReadOnly() && m_lastInsertPoint.isValid()) { cursor = m_lastInsertPoint; setMode(Insert); finish(cursor); }
            resetCommand(); return true;
        }
        if (key == ';' || key == ',') {
            if (!m_changes.isEmpty()) {
                if (m_changeIndex < 0) m_changeIndex = m_changes.size();
                m_changeIndex = qBound(0, m_changeIndex + (key == ';' ? -count : count), int(m_changes.size()) - 1);
                finish(m_changes.at(m_changeIndex));
            }
            resetCommand(); return true;
        }

        if (key == 'j' || key == 'k' || key == '0' || key == '^' || key == '$' || key == '_' || key == 'm' || key == 'o') {
            if (key == 'j' || key == 'k') cursor.movePosition(count, key == 'j' ? QDocumentCursor::Down : QDocumentCursor::Up, QDocumentCursor::ThroughWrap);
            else if (key == 'o') setOffset(cursor, count - 1);
            else if (key == '_') {
                cursor.movePosition(count - 1, QDocumentCursor::Down);
                const QString text = cursor.line().text(); int end = text.size();
                while (end > 0 && text.at(end - 1).isSpace()) --end;
                cursor.setColumnNumber(qMax(0, end - 1));
            } else {
                const QDocumentLine line = cursor.line();
                const int row = line.wrappedLineForCursor(cursor.columnNumber());
                int first = cursor.columnNumber(), last = first;
                while (first > 0 && line.wrappedLineForCursor(first - 1) == row) --first;
                while (last + 1 < line.length() && line.wrappedLineForCursor(last + 1) == row) ++last;
                if (key == '^') while (first < last && line.text().at(first).isSpace()) ++first;
                cursor.setColumnNumber(key == '$' ? last : key == 'm' ? (first + last) / 2 : first);
            }
            applyMotion(cursor, key == 'j' ? 'l' : key == 'k' ? 'h' : key, key == '$' || key == '_');
            resetCommand(); return true;
        }
        if (key == '*' || key == '#') {
            QDocumentCursor object = cursor;
            if (textObject(object, 'w', false, 1)) m_searchPattern = QRegularExpression::escape(object.selectedText());
            m_searchBackward = key == '#';
            if (search(cursor, m_searchPattern, m_searchBackward, count)) applyMotion(cursor, 'n');
            resetCommand(); return true;
        }

        if (key == 'v') {
            if (m_lastVisualAnchor.isValid() && m_lastVisualCursor.isValid()) {
                m_visualAnchor = m_lastVisualAnchor; m_visualCursor = m_lastVisualCursor;
                setMode(m_lastVisualMode); showVisualSelection();
            }
            resetCommand(); return true;
        }
        if (key == 't' || key == 'T' || key == 'f') {
            QString arg;
            if (key == 'f') { QDocumentCursor object = cursor; if (textObject(object, 'W', false, 1)) arg = object.selectedText(); }
            const QString cmd = key == 'f' ? "e" : key == 't' ? "bn" : "bp";
            QMetaObject::invokeMethod(this, [this, cmd, arg] { emit applicationCommand(cmd, arg); }, Qt::QueuedConnection);
            resetCommand(); return true;
        }
        if (key == 'e' || key == 'E') {
            for (int i = 0; i < count; ++i) {
                const bool big = key == 'E';
                auto category = [big](QChar ch) { return big && !ch.isSpace() && !ch.isNull() ? 1 : wordClass(ch); };
                const int original = category(cursor.nextChar());
                const int previous = category(cursor.previousChar());
                if (!cursor.atStart()) cursor.movePosition(1, QDocumentCursor::PreviousCharacter);
                if (previous == original && original != 0) {
                    while (!cursor.atStart() && category(cursor.previousChar()) == original) cursor.movePosition(1, QDocumentCursor::PreviousCharacter);
                    if (!cursor.atStart()) cursor.movePosition(1, QDocumentCursor::PreviousCharacter);
                }
                while (!cursor.atStart() && category(cursor.nextChar()) == 0) cursor.movePosition(1, QDocumentCursor::PreviousCharacter);
            }
            applyMotion(cursor, key, true); resetCommand(); return true;
        }
        if (key != 'g') { resetCommand(); return true; }
    } else if (key == 'g') { m_g = true; return true; }
    if ((key == 'i' || key == 'a') && (!m_operator.isNull() || visual())) {
        m_await = key; m_pendingCount = count; return true;
    }
    if (key == '"' || key == 'f' || key == 'F' || key == 't' || key == 'T' || key == 'r'
        || key == 'm' || key == '\'' || key == '`' || key == 'z' || key == 'Z' || key == '@') {
        m_await = key; m_pendingCount = count; m_pendingExplicitCount = explicitCount; return true;
    }
    if (key == 'q' && m_operator.isNull() && !visual()) {
        if (!m_recording.isNull()) { m_macros[m_recording] = m_recorded; m_recording = QChar(); resetCommand(); updateStatus(); }
        else m_await = 'q';
        return true;
    }
    if (key == ':' || key == '/' || key == '?') { enterCommand(key); return true; }
    if (key == '.') {
        const QList<Key> change = m_lastChange;
        m_candidate.clear(); resetCommand(); replay(change, count); return true;
    }
    if (key == ';' || key == ',') {
        QChar kind = m_findKind;
        if (key == ',') kind = kind.isUpper() ? kind.toLower() : kind.toUpper();
        if (kind == 't' && cursor.columnNumber() + 1 < cursor.line().length() && cursor.line().text().at(cursor.columnNumber() + 1) == m_findTarget)
            cursor.movePosition(1, QDocumentCursor::NextCharacter);
        if (kind == 'T' && cursor.columnNumber() > 0 && cursor.line().text().at(cursor.columnNumber() - 1) == m_findTarget)
            cursor.movePosition(1, QDocumentCursor::PreviousCharacter);
        if (!kind.isNull() && findCharacter(cursor, kind, m_findTarget, count)) applyMotion(cursor, kind, true);
    } else if (key == 'n' || key == 'N' || key == '*' || key == '#') {
        if (key == '*' || key == '#') {
            QDocumentCursor object = cursor;
            if (textObject(object, 'w', false, 1)) m_searchPattern = "\\b" + QRegularExpression::escape(object.selectedText()) + "\\b";
            m_searchBackward = key == '#';
        }
        if (search(cursor, m_searchPattern, key == 'N' ? !m_searchBackward : m_searchBackward, count)) applyMotion(cursor, 'n');
    } else if (QStringLiteral("dcy><").contains(key) && !key.isNull()) {
        if (visual()) {
            if (m_mode == VisualBlock && QStringLiteral("dcy").contains(key)) operateBlock(key);
            else if (key == '>' || key == '<') {
                if (m_mode == VisualBlock) message(tr("Block shifts are not implemented"));
                else transform(m_editor->cursor(), key, count);
            }
            else operate(m_editor->cursor(), key, m_mode == VisualLine);
        } else if (!m_operator.isNull()) {
            if (key == m_operator) operate(lines(cursor.lineNumber(), cursor.lineNumber() + count - 1), key, true);
        } else { m_operator = key; m_operatorCount = count; m_count = 0; return true; }
    } else if (!m_operator.isNull() && key == m_operator) {
        operate(lines(cursor.lineNumber(), cursor.lineNumber() + count - 1), key, true);
    } else if (move(cursor, key, count, explicitCount)) {
        if (m_operator == 'c' && (key == 'w' || key == 'W') && wordClass(m_editor->cursor().nextChar()) != 0) {
            cursor = m_editor->cursor(); move(cursor, key == 'w' ? 'e' : 'E', count, explicitCount);
            applyMotion(cursor, key, true);
        } else {
            if (key == 'g' || key == 'G' || key == '%') addJump();
            applyMotion(cursor, key, key == 'e' || key == 'E' || key == '%');
        }
    } else if (!m_operator.isNull()) {
        // Unsupported pending sequences never fall through to native text input.
    } else if (key == 'v' || key == 'V') {
        const Mode target = key == 'v' ? Visual : VisualLine;
        if (m_mode == target) { setMode(Normal); finish(cursor); }
        else { m_visualAnchor = cursor; m_visualCursor = cursor; setMode(target); showVisualSelection(); }
    } else if (visual() && key == 'o') { qSwap(m_visualAnchor, m_visualCursor); showVisualSelection(); }
    else if (key == 'J') join(count, true);
    else if (key == '~' || (visual() && (key == 'u' || key == 'U'))) {
        if (visual()) { if (m_mode == VisualBlock) operateBlock(key); else transform(m_editor->cursor(), key); }
        else {
            QDocumentCursor end = cursor;
            end.setColumnNumber(qMin(end.line().length(), end.columnNumber() + count));
            cursor.moveTo(end, QDocumentCursor::KeepAnchor); transform(cursor, key); finish(end);
        }
    } else if (visual() && (key == 'p' || key == 'P')) {
        Register value = readRegister();
        if (value.text.isEmpty()) { resetCommand(); return true; }
        if (!m_editor->isReadOnly()) {
            if (m_mode == VisualBlock) {
                if (value.kind == 1) { message(tr("Use a character or block register for block paste")); resetCommand(); return true; }
                if (value.kind == 0 && !value.text.contains('\n')) {
                    QStringList rows;
                    for (int i = 0; i <= qAbs(m_visualAnchor.lineNumber() - m_visualCursor.lineNumber()); ++i) rows.append(value.text);
                    value = {rows.join('\n'), 2};
                } else value.kind = 2;
                const bool replaying = m_replaying; m_replaying = true;
                m_editor->document()->beginMacro();
                operateBlock('d'); paste(true, count, &value);
                m_editor->document()->endMacro();
                m_replaying = replaying;
                setMode(Normal); resetCommand(); return true;
            }
            QDocumentCursor selection = m_editor->cursor(), start = selection.selectionStart();
            selection.beginEditBlock(); selection.insertText(value.text); selection.endEditBlock();
            setMode(Normal); finish(start);
        }
    } else if (m_mode == VisualBlock && (key == 'I' || key == 'A')) {
        if (!m_editor->isReadOnly()) {
            const int top = qMin(m_visualAnchor.lineNumber(), m_visualCursor.lineNumber()), bottom = qMax(m_visualAnchor.lineNumber(), m_visualCursor.lineNumber());
            const int column = key == 'I' ? qMin(m_visualAnchor.columnNumber(), m_visualCursor.columnNumber()) : qMax(m_visualAnchor.columnNumber(), m_visualCursor.columnNumber()) + 1;
            setMode(Insert);
            finish(QDocumentCursor(m_editor->document(), top, qMin(column, m_editor->document()->line(top).length())));
            for (int line = top + 1; line <= bottom; ++line) m_editor->addCursorMirror(QDocumentCursor(m_editor->document(), line, qMin(column, m_editor->document()->line(line).length())));
        }
    } else if (visual() && (key == 'x' || key == 's' || key == 'S' || key == 'D' || key == 'C' || key == 'Y')) {
        const QChar op = key == 'Y' ? QChar('y') : key == 's' || key == 'S' || key == 'C' ? QChar('c') : QChar('d');
        if (m_mode == VisualBlock) operateBlock(op); else operate(m_editor->cursor(), op, m_mode == VisualLine);
    } else if (visual()) {
    } else if (QStringLiteral("iaIAoOR").contains(key) && !key.isNull()) {
        if (m_editor->isReadOnly()) { resetCommand(); return true; }
        beginInsert();
        if (key == 'a' && !cursor.atLineEnd()) cursor.movePosition(1, QDocumentCursor::NextCharacter);
        if (key == 'I') cursor.movePosition(1, QDocumentCursor::StartOfLineText);
        if (key == 'A') cursor.movePosition(1, QDocumentCursor::EndOfLine);
        if (key == 'o' || key == 'O') {
            const QString line = cursor.line().text();
            int indent = 0; while (indent < line.size() && line.at(indent).isSpace()) ++indent;
            const QString prefix = m_editor->flag(QEditor::AutoIndent) ? line.left(indent) : QString();
            cursor.movePosition(1, key == 'o' ? QDocumentCursor::EndOfLine : QDocumentCursor::StartOfLine);
            cursor.insertText(key == 'o' ? "\n" + prefix : prefix + "\n");
            if (key == 'O') cursor.moveTo(cursor.lineNumber() - 1, prefix.size());
        }
        setMode(key == 'R' ? Replace : Insert);
        m_insertRepeat = count; m_openRepeat = key == 'o' || key == 'O';
        finish(cursor);
    } else if (QStringLiteral("xsXDCYS").contains(key) && !key.isNull()) {
        if (key == 'Y' || key == 'S') operate(lines(cursor.lineNumber(), cursor.lineNumber() + count - 1), key == 'Y' ? 'y' : 'c', true);
        else {
            QDocumentCursor end = cursor;
            if (key == 'D' || key == 'C') move(end, '$', count, explicitCount);
            else if (key == 'X') end.setColumnNumber(qMax(0, end.columnNumber() - count));
            else end.setColumnNumber(qMin(end.line().length(), end.columnNumber() + count));
            cursor.moveTo(end, QDocumentCursor::KeepAnchor);
            operate(cursor, key == 's' || key == 'C' ? 'c' : 'd', false);
        }
    } else if (key == 'p' || key == 'P') paste(key == 'P', count);
    else if (key == 'u') {
        m_ignoreChange = true;
        if (!m_editor->isReadOnly()) for (int i = 0; i < count; ++i) m_editor->undo();
        finish(m_editor->cursor());
    }
    resetCommand(); return true;
}
