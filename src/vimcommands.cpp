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
#include <QLineEdit>
#include <QMimeData>
#include <QRegularExpression>
#include <QScrollBar>
#include <QFile>
#include <algorithm>
#include <limits>

QHash<QChar, VimInputBinding::Register> VimInputBinding::m_registers;
QHash<QChar, QList<VimInputBinding::Key>> VimInputBinding::m_macros;

namespace {
int category(QChar ch, bool big = false)
{
    if (ch.isNull() || ch.isSpace()) return 0;
    return big || ch.isLetterOrNumber() || ch == '_' ? 1 : 2;
}
bool escaped(const QString &text, int position)
{
    int slashes = 0;
    while (position > 0 && text.at(--position) == '\\') ++slashes;
    return slashes % 2;
}
}

bool VimInputBinding::visual() const { return m_mode == Visual || m_mode == VisualLine || m_mode == VisualBlock; }
bool VimInputBinding::inserting() const { return m_mode == Insert || m_mode == Replace; }

QString VimInputBinding::bufferText() const
{
    // Cursor offsets count one separator per logical line. Document::text()
    // uses the file's line ending and can return two characters for CRLF.
    return m_editor->document()->textLines().join('\n');
}

int VimInputBinding::offset(const QDocumentCursor &cursor) const
{
    int result = cursor.columnNumber();
    for (int line = 0; line < cursor.lineNumber(); ++line) result += m_editor->document()->line(line).length() + 1;
    return result;
}
void VimInputBinding::setOffset(QDocumentCursor &cursor, int position, QDocumentCursor::MoveMode mode) const
{
    position = qMax(0, position);
    int line = 0;
    while (line + 1 < m_editor->document()->lines() && position > m_editor->document()->line(line).length()) {
        position -= m_editor->document()->line(line).length() + 1;
        ++line;
    }
    cursor.moveTo(line, qMin(position, m_editor->document()->line(line).length()), mode);
}

void VimInputBinding::updateStatus()
{
    QString label = m_mode == Insert ? tr("INSERT") : m_mode == Replace ? tr("REPLACE")
                  : m_mode == Visual ? tr("VISUAL") : m_mode == VisualLine ? tr("VISUAL LINE")
                  : m_mode == VisualBlock ? tr("VISUAL BLOCK") : m_mode == Command ? QString(m_prompt) + m_commandText : tr("NORMAL");
    if (!m_recording.isNull()) label += tr(" (recording @%1)").arg(m_recording);
    m_editor->setProperty("vimMode", QStringLiteral("VIM ") + label);
    emit modeChanged();
}

void VimInputBinding::message(const QString &text)
{
    m_editor->setProperty("vimMessage", text);
    m_commandLine->setText(text);
    m_commandLine->setGeometry(0, qMax(0, m_editor->viewport()->height() - m_commandLine->sizeHint().height()),
                               m_editor->viewport()->width(), m_commandLine->sizeHint().height());
    m_commandLine->show();
    m_commandLine->raise();
}

void VimInputBinding::recordKey(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Shift || event->key() == Qt::Key_Control
        || event->key() == Qt::Key_Alt || event->key() == Qt::Key_Meta) return;
    const Key key{event->key(), event->modifiers(), event->text()};
    if (inserting() && event->key() != Qt::Key_Escape
        && !(event->modifiers() == Qt::ControlModifier && (event->key() == Qt::Key_BracketLeft || event->key() == Qt::Key_C))) m_insertSequence.append(key);
    if (m_replaying) return;
    if (!m_recording.isNull() && !(m_mode == Normal && m_await.isNull() && m_operator.isNull()
                                     && !m_g && event->text() == "q")) m_recorded.append(key);
    if (m_candidate.isEmpty()) m_beforeChange = bufferText();
    m_candidate.append(key);
}

void VimInputBinding::completeChange()
{
    if (m_mode != Normal || m_replaying) return;
    if (!m_ignoreChange && !m_candidate.isEmpty() && bufferText() != m_beforeChange)
        m_lastChange = m_candidate;
    m_candidate.clear();
    m_ignoreChange = false;
}

void VimInputBinding::beginInsert()
{
    if (!m_insertBlock && !m_editor->isReadOnly()) {
        m_editor->document()->beginMacro();
        m_insertBlock = true;
    }
}
void VimInputBinding::endInsert()
{
    if (m_insertBlock) {
        const QString current = bufferText();
        int first = 0, suffix = 0;
        while (first < current.size() && first < m_insertBase.size() && current.at(first) == m_insertBase.at(first)) ++first;
        while (suffix < current.size() - first && suffix < m_insertBase.size() - first
            && current.at(current.size() - suffix - 1) == m_insertBase.at(m_insertBase.size() - suffix - 1)) ++suffix;
        m_registers['.'] = {current.mid(first, current.size() - first - suffix), 0};
        m_insertBlock = false;
        m_editor->document()->endMacro();
    }
}

void VimInputBinding::replay(const QList<Key> &keys, int count)
{
    if (keys.isEmpty()) return;
    if (m_replayDepth >= 10) { message(tr("Macro recursion limit reached")); return; }
    const QList<Key> sequence = keys;
    const bool previous = m_replaying;
    if (!m_replayDepth) m_replayBudget = 100000;
    m_replaying = true;
    ++m_replayDepth;
    if (!inserting()) m_candidate.clear();
    for (int n = 0; n < count && m_replayBudget > 0; ++n) {
        for (const Key &key : sequence) {
            if (--m_replayBudget < 0) break;
            QKeyEvent event(QEvent::KeyPress, key.key, key.modifiers, key.text);
            QApplication::sendEvent(m_editor, &event);
        }
    }
    --m_replayDepth;
    m_replaying = previous;
    if (m_replayBudget <= 0) message(tr("Macro execution limit reached"));
}

void VimInputBinding::storeRegister(const QString &text, int kind, QChar op)
{
    if (m_register == '_') return;
    const Register value{text, kind};
    const QChar selected = m_register.isUpper() ? m_register.toLower() : m_register;
    if (m_register.isUpper()) {
        Register &reg = m_registers[selected];
        reg.text += text;
        reg.kind = kind;
    } else m_registers[selected] = value;
    m_registers['"'] = value;
    if (op == 'y') m_registers['0'] = value;
    else if (op == 'd' || op == 'c') {
        if (kind == 1 || text.contains('\n')) {
            for (int i = 9; i > 1; --i) m_registers[QChar('0' + i)] = m_registers.value(QChar('0' + i - 1));
            m_registers['1'] = value;
        } else m_registers['-'] = value;
    }
    auto *mime = new QMimeData;
    mime->setText(text);
    if (kind == 1) mime->setData("application/x-texstudio-vim-linewise", "1");
    if (kind == 2) mime->setData("application/x-texstudio-vim-blockwise", "1");
    QApplication::clipboard()->setMimeData(mime);
}

VimInputBinding::Register VimInputBinding::readRegister() const
{
    if (m_register == '_') return {};
    if (m_register == '"' || m_register == '+' || m_register == '*') {
        const QMimeData *mime = QApplication::clipboard()->mimeData();
        if (!mime) return {};
        return {mime->text(), mime->hasFormat("application/x-texstudio-vim-blockwise") ? 2
                             : mime->hasFormat("application/x-texstudio-vim-linewise") ? 1 : 0};
    }
    return m_registers.value(m_register.toLower());
}

void VimInputBinding::enterCommand(QChar prompt)
{
    m_commandPreviousMode = m_mode;
    m_rangeFirst = m_rangeLast = -1;
    if (visual()) {
        m_rangeFirst = qMin(m_visualAnchor.lineNumber(), m_visualCursor.lineNumber());
        m_rangeLast = qMax(m_visualAnchor.lineNumber(), m_visualCursor.lineNumber());
    }
    m_searchOperator = m_operator;
    m_searchCount = qMin(10000, qMax(1, m_count) * m_operatorCount);
    m_mode = Command;
    m_prompt = prompt;
    m_commandText.clear();
    m_commandPosition = 0;
    m_historyPosition = m_history[prompt].size();
    message(QString(prompt));
    m_commandLine->setReadOnly(true);
    updateStatus();
}

bool VimInputBinding::commandKey(QKeyEvent *event)
{
    const bool control = event->modifiers() == Qt::ControlModifier;
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        const QString command = m_commandText;
        if (!command.isEmpty()) {
            QStringList &history = m_history[m_prompt];
            if (history.isEmpty() || history.last() != command) history.append(command);
            if (history.size() > 100) history.removeFirst();
        }
        m_commandLine->hide();
        m_mode = Normal;
        if (m_prompt == ':') { m_candidate.clear(); executeEx(command); m_candidate.clear(); }
        else {
            if (!command.isEmpty()) m_searchPattern = command;
            m_searchBackward = m_prompt == '?';
            QDocumentCursor cursor = m_commandPreviousMode == Visual || m_commandPreviousMode == VisualLine
                || m_commandPreviousMode == VisualBlock ? m_visualCursor : m_editor->cursor();
            cursor.clearSelection();
            if (search(cursor, m_searchPattern, m_searchBackward, m_searchCount)) {
                m_mode = m_commandPreviousMode;
                m_operator = m_searchOperator;
                applyMotion(cursor, 'n');
            }
        }
        if (m_mode == Normal) resetCommand();
        updateStatus();
        return true;
    }
    if (event->key() == Qt::Key_Left) m_commandPosition = qMax(0, m_commandPosition - 1);
    else if (event->key() == Qt::Key_Right) m_commandPosition = qMin(int(m_commandText.size()), m_commandPosition + 1);
    else if (event->key() == Qt::Key_Home || (control && event->key() == Qt::Key_B)) m_commandPosition = 0;
    else if (event->key() == Qt::Key_End || (control && event->key() == Qt::Key_E)) m_commandPosition = m_commandText.size();
    else if (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) {
        const QStringList history = m_history.value(m_prompt);
        m_historyPosition = qBound(0, m_historyPosition + (event->key() == Qt::Key_Up ? -1 : 1), int(history.size()));
        m_commandText = m_historyPosition < history.size() ? history.at(m_historyPosition) : QString();
        m_commandPosition = m_commandText.size();
    } else if (control && event->key() == Qt::Key_U) {
        m_commandText.remove(0, m_commandPosition); m_commandPosition = 0;
    } else if (event->key() == Qt::Key_Backspace) {
        if (m_commandPosition > 0) m_commandText.remove(--m_commandPosition, 1);
    } else if (event->key() == Qt::Key_Delete) m_commandText.remove(m_commandPosition, 1);
    else if (!(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
        m_commandText.insert(m_commandPosition, event->text());
        m_commandPosition += event->text().size();
    }
    message(QString(m_prompt) + m_commandText);
    m_commandLine->setCursorPosition(m_commandPosition + 1);
    updateStatus();
    return true;
}

bool VimInputBinding::search(QDocumentCursor &cursor, const QString &pattern, bool backward, int count)
{
    if (pattern.isEmpty()) { message(tr("No previous search pattern")); return false; }
    QString expression = pattern;
    expression.replace("\\<", "\\b").replace("\\>", "\\b");
    QRegularExpression::PatternOptions flags;
    if (m_ignoreCase && !(m_smartCase && pattern.contains(QRegularExpression("[A-Z]")))) flags |= QRegularExpression::CaseInsensitiveOption;
    QRegularExpression regex(expression, flags);
    if (!regex.isValid()) { message(regex.errorString()); return false; }
    QList<int> positions;
    auto matches = regex.globalMatch(bufferText());
    while (matches.hasNext()) positions.append(matches.next().capturedStart());
    if (positions.isEmpty()) { message(tr("Pattern not found: %1").arg(pattern)); return false; }
    int index;
    if (backward) {
        index = int(std::lower_bound(positions.cbegin(), positions.cend(), offset(cursor)) - positions.cbegin()) - count;
    } else {
        index = int(std::upper_bound(positions.cbegin(), positions.cend(), offset(cursor)) - positions.cbegin()) + count - 1;
    }
    index = (index % positions.size() + positions.size()) % positions.size();
    addJump();
    setOffset(cursor, positions.at(index));
    return true;
}

bool VimInputBinding::findCharacter(QDocumentCursor &cursor, QChar kind, QChar target, int count)
{
    const QString line = cursor.line().text();
    int position = cursor.columnNumber();
    const bool backward = kind == 'F' || kind == 'T';
    for (int i = 0; i < count; ++i) {
        if (backward && position <= 0) return false;
        position = backward ? line.lastIndexOf(target, position - 1) : line.indexOf(target, position + 1);
        if (position < 0) return false;
    }
    if (kind == 't') --position;
    if (kind == 'T') ++position;
    cursor.setColumnNumber(position);
    return true;
}

bool VimInputBinding::textObject(QDocumentCursor &selection, QChar object, bool around, int count)
{
    const QString text = bufferText();
    if (text.isEmpty()) return !around && count == 1 && (object == 'w' || object == 'W');
    int pos = qMin(offset(selection), int(text.size()) - 1), first = pos, last = pos + 1;
    if (object == 'w' || object == 'W') {
        const bool big = object == 'W';
        // iw counts both words and intervening horizontal whitespace. aw
        // counts only words; starting in whitespace includes the following word.
        const auto horizontal = [&](int i) { return i >= 0 && i < text.size() && text.at(i).isSpace() && text.at(i) != '\n'; };
        const int type = category(text.at(pos), big);
        while (first > 0 && text.at(first - 1) != '\n' && category(text.at(first - 1), big) == type) --first;
        last = pos;
        if (text.at(pos) != '\n') {
            while (last < text.size() && text.at(last) != '\n' && category(text.at(last), big) == type) ++last;
        }
        int remaining = count - 1;
        if (around && type == 0) ++remaining;
        while (remaining > 0) {
            int next = last;
            if (around) while (next < text.size() && text.at(next).isSpace()) ++next;
            else if (next < text.size() && text.at(next) == '\n') ++next;
            if (next >= text.size()) return false;
            const int nextType = category(text.at(next), big);
            last = next;
            while (last < text.size() && text.at(last) != '\n' && category(text.at(last), big) == nextType) ++last;
            --remaining;
        }
        if (around) {
            if (type != 0) {
                const int end = last;
                while (horizontal(last)) ++last;
                if (last == end) while (horizontal(first - 1)) --first;
            } else if (count > 1 && last > 0 && category(text.at(last - 1), big) == 2) while (horizontal(last)) ++last;
        }
    } else if (object == 'p') {
        int start = selection.lineNumber(), end = start;
        const auto blank = [&](int line) { return m_editor->document()->line(line).text().trimmed().isEmpty(); };
        const int total = m_editor->document()->lines();
        const bool startedBlank = blank(start);
        while (start > 0 && blank(start - 1) == startedBlank) --start;
        while (end + 1 < total && blank(end + 1) == startedBlank) ++end;
        int remaining = count - 1 + (around && startedBlank ? 1 : 0);
        while (remaining-- > 0) {
            if (end + 1 >= total) return false;
            ++end;
            if (around) while (end < total && blank(end)) ++end;
            if (end >= total) return false;
            const bool type = blank(end);
            while (end + 1 < total && blank(end + 1) == type) ++end;
        }
        if (around && !startedBlank) {
            const int before = end;
            while (end + 1 < total && blank(end + 1)) ++end;
            if (end == before) while (start > 0 && blank(start - 1)) --start;
        }
        selection = lines(start, end);
        return true;
    } else if (object == 's') {
        QList<QPair<int, int>> units;
        for (int i = 0; i < text.size();) {
            const int begin = i;
            if (text.at(i).isSpace()) {
                while (i < text.size() && text.at(i).isSpace()) ++i;
            } else {
                while (i < text.size()) {
                    if (QStringLiteral(".!?").contains(text.at(i))) {
                        int end = i + 1;
                        while (end < text.size() && QStringLiteral(")]\"'").contains(text.at(end))) ++end;
                        if (end == text.size() || text.at(end).isSpace()) { i = end; break; }
                    }
                    ++i;
                }
            }
            units.append(qMakePair(begin, i));
        }
        int index = 0;
        while (index + 1 < units.size() && units.at(index).second <= pos) ++index;
        first = units.at(index).first;
        const bool startedBlank = text.at(first).isSpace();
        int remaining = count - 1 + (around && startedBlank ? 1 : 0);
        while (remaining-- > 0) {
            ++index;
            if (around && index < units.size() && text.at(units.at(index).first).isSpace()) ++index;
            if (index >= units.size()) { index = units.size() - 1; break; }
        }
        last = units.at(index).second;
        if (around && !startedBlank) {
            const int before = last;
            while (last < text.size() && text.at(last).isSpace() && text.at(last) != '\n') ++last;
            if (last == before) while (first > 0 && text.at(first - 1).isSpace() && text.at(first - 1) != '\n') --first;
        }
    } else if (object == 't') {
        struct Tag { QString name; int start; int end; };
        QList<Tag> stack;
        struct Block { int first; int innerFirst; int innerLast; int last; };
        QList<Block> blocks;
        QRegularExpression tags("<!--[\\s\\S]*?-->|<(/?)([A-Za-z][\\w:.-]*)(?:[^>\"']|\"[^\"]*\"|'[^']*')*>");
        auto matches = tags.globalMatch(text);
        while (matches.hasNext()) {
            const auto match = matches.next();
            if (match.captured().startsWith("<!--") || match.captured().endsWith("/>")) continue;
            const QString name = match.captured(2).toLower();
            if (match.captured(1).isEmpty()) stack.append({name, int(match.capturedStart()), int(match.capturedEnd())});
            else {
                int index = stack.size() - 1;
                while (index >= 0 && stack.at(index).name != name) --index;
                if (index < 0) continue;
                const Tag open = stack.at(index);
                stack = stack.mid(0, index);
                if (open.start <= pos && pos < match.capturedEnd()) blocks.append({open.start, open.end, int(match.capturedStart()), int(match.capturedEnd())});
            }
        }
        std::sort(blocks.begin(), blocks.end(), [](const Block &a, const Block &b) { return a.last - a.first < b.last - b.first; });
        if (blocks.size() < count) return false;
        const Block block = blocks.at(count - 1);
        first = around ? block.first : block.innerFirst;
        last = around ? block.last : block.innerLast;
    } else if (object == '"' || object == '\'' || object == '`') {
        const int lineStart = pos - selection.columnNumber();
        const int lineEnd = lineStart + selection.line().length();
        int open = -1, close = -1;
        QList<int> quotes;
        for (int i = lineStart; i < lineEnd; ++i) {
            if (text.at(i) != object || escaped(text, i)) continue;
            quotes.append(i);
        }
        for (int i = 0; i + 1 < quotes.size(); ++i) {
            if (pos < quotes.first() || (quotes.at(i) <= pos && pos < quotes.at(i + 1)) || pos == quotes.at(i + 1)) {
                int index = i;
                if (quotes.contains(pos)) { index = quotes.indexOf(pos); index -= index % 2; }
                if (index + 1 < quotes.size()) { open = quotes.at(index); close = quotes.at(index + 1); }
                break;
            }
        }
        if (open < 0 || close < 0) return false;
        first = open + (around || count > 1 ? 0 : 1); last = close + (around || count > 1 ? 1 : 0);
        if (around) {
            const int end = last;
            while (last < lineEnd && text.at(last).isSpace()) ++last;
            if (last == end) while (first > lineStart && text.at(first - 1).isSpace()) --first;
        }
    } else {
        QString opens("([{<"), closes(")]}>" );
        if (object == 'b') object = '(';
        if (object == 'B') object = '{';
        int pair = opens.indexOf(object);
        if (pair < 0) pair = closes.indexOf(object);
        if (pair < 0) return false;
        QList<int> stack;
        QList<QPair<int, int>> enclosing;
        for (int i = 0; i < text.size(); ++i) {
            if (escaped(text, i)) continue;
            if (text.at(i) == opens.at(pair)) stack.append(i);
            if (text.at(i) == closes.at(pair) && !stack.isEmpty()) {
                const int open = stack.takeLast();
                const bool skipOpening = around && visual() && m_visualAnchor != m_visualCursor && open == pos
                    && pos == offset(m_editor->cursor().selectionStart());
                if (open <= pos && pos <= i && !skipOpening) enclosing.append(qMakePair(open, i));
            }
        }
        std::sort(enclosing.begin(), enclosing.end(), [](const QPair<int,int> &a, const QPair<int,int> &b) { return a.second - a.first < b.second - b.first; });
        if (enclosing.isEmpty()) {
            // Find the next block on the current line when outside a block.
            const int next = text.indexOf(opens.at(pair), pos + 1);
            const int lineEnd = text.indexOf('\n', pos);
            if (next > pos && (lineEnd < 0 || next < lineEnd)) {
                QDocumentCursor upcoming = selection; setOffset(upcoming, next);
                if (textObject(upcoming, object, around, count)) { selection = upcoming; return true; }
            }
        }
        if (enclosing.size() < count) return false;
        first = enclosing.at(count - 1).first + (around ? 0 : 1);
        last = enclosing.at(count - 1).second + (around ? 1 : 0);
        if (!around && first < last && text.at(first) == '\n') {
            ++first;
            int indent = last;
            while (indent > first && text.at(indent - 1).isSpace() && text.at(indent - 1) != '\n') --indent;
            if (indent > first && text.at(indent - 1) == '\n') last = indent;
        }
    }
    setOffset(selection, first);
    setOffset(selection, last, QDocumentCursor::KeepAnchor);
    return true;
}

void VimInputBinding::applyMotion(QDocumentCursor cursor, QChar motion, bool inclusive)
{
    if (!m_operator.isNull()) {
        QDocumentCursor start = m_editor->cursor();
        start.clearSelection();
        const bool linewise = QStringLiteral("jkgG'{}HL").contains(motion) && !motion.isNull();
        if (linewise) start = lines(qMin(start.lineNumber(), cursor.lineNumber()), qMax(start.lineNumber(), cursor.lineNumber()));
        else {
            if (inclusive) {
                if (cursor < start) {
                    if (!start.atLineEnd()) start.movePosition(1, QDocumentCursor::NextCharacter);
                } else if (!cursor.atLineEnd()) cursor.movePosition(1, QDocumentCursor::NextCharacter);
            }
            start.moveTo(cursor, QDocumentCursor::KeepAnchor);
        }
        operate(start, m_operator, linewise);
    } else if (visual()) {
        cursor.clearSelection();
        cursor.setColumnNumber(qMin(cursor.columnNumber(), qMax(0, cursor.line().length() - 1)));
        m_visualCursor = cursor;
        showVisualSelection();
    } else finish(cursor);
}

void VimInputBinding::transform(QDocumentCursor selection, QChar op, int count)
{
    if (m_editor->isReadOnly()) return;
    const QDocumentCursor start = selection.selectionStart();
    if (op == '>' || op == '<') {
        int first = selection.selectionStart().lineNumber(), last = selection.selectionEnd().lineNumber();
        if (last > first && selection.selectionEnd().columnNumber() == 0) --last;
        selection.beginEditBlock();
        for (int line = last; line >= first; --line) {
            QDocumentCursor cursor(m_editor->document(), line, 0);
            const int width = qMax(1, QDocument::tabStop()) * count;
            if (op == '>') cursor.insertText(m_editor->flag(QEditor::ReplaceIndentTabs) ? QString(width, ' ') : QString(count, '\t'));
            else {
                const QString text = cursor.line().text();
                int remove = 0, columns = 0;
                while (remove < text.size() && columns < width && (text.at(remove) == ' ' || text.at(remove) == '\t')) {
                    columns += text.at(remove) == '\t' ? qMax(1, QDocument::tabStop()) : 1;
                    ++remove;
                }
                cursor.movePosition(remove, QDocumentCursor::NextCharacter, QDocumentCursor::KeepAnchor);
                cursor.removeSelectedText();
            }
        }
        selection.endEditBlock();
    } else {
        QString text = selection.selectedText();
        if (op == 'U') text = text.toUpper();
        else if (op == 'u') text = text.toLower();
        else for (int i = 0; i < text.size(); ++i) text[i] = text.at(i).isUpper() ? text.at(i).toLower() : text.at(i).toUpper();
        selection.beginEditBlock();
        selection.insertText(text);
        selection.endEditBlock();
    }
    setMode(Normal);
    finish(start);
}

void VimInputBinding::operateBlock(QChar op)
{
    if (op != 'y' && m_editor->isReadOnly()) return;
    const int top = qMin(m_visualAnchor.lineNumber(), m_visualCursor.lineNumber());
    const int bottom = qMax(m_visualAnchor.lineNumber(), m_visualCursor.lineNumber());
    const int left = qMin(m_visualAnchor.columnNumber(), m_visualCursor.columnNumber());
    const int right = qMax(m_visualAnchor.columnNumber(), m_visualCursor.columnNumber()) + 1;
    if (op == 'u' || op == 'U' || op == '~') {
        QDocumentCursor transaction(m_editor->document(), top, left);
        transaction.beginEditBlock();
        for (int line = bottom; line >= top; --line) {
            QDocumentCursor part(m_editor->document(), line, qMin(left, m_editor->document()->line(line).length()));
            part.moveTo(line, qMin(right, part.line().length()), QDocumentCursor::KeepAnchor);
            QString text = part.selectedText();
            if (op == 'u') text = text.toLower();
            else if (op == 'U') text = text.toUpper();
            else for (int i = 0; i < text.size(); ++i) text[i] = text.at(i).isUpper() ? text.at(i).toLower() : text.at(i).toUpper();
            part.insertText(text);
        }
        transaction.endEditBlock();
        setMode(Normal); finish(QDocumentCursor(m_editor->document(), top, left));
        return;
    }
    QStringList pieces;
    for (int line = top; line <= bottom; ++line) pieces.append(m_editor->document()->line(line).text().mid(left, right - left));
    storeRegister(pieces.join('\n'), 2, op);
    if (op == 'c') beginInsert();
    QDocumentCursor transaction(m_editor->document(), top, left);
    if (op != 'y') {
        transaction.beginEditBlock();
        for (int line = bottom; line >= top; --line) {
            QDocumentCursor c(m_editor->document(), line, qMin(left, m_editor->document()->line(line).length()));
            c.moveTo(line, qMin(right, c.line().length()), QDocumentCursor::KeepAnchor);
            c.removeSelectedText();
        }
        transaction.endEditBlock();
    }
    setMode(op == 'c' ? Insert : Normal);
    finish(QDocumentCursor(m_editor->document(), top, qMin(left, m_editor->document()->line(top).length())));
    if (op == 'c') for (int line = top + 1; line <= bottom; ++line)
        m_editor->addCursorMirror(QDocumentCursor(m_editor->document(), line, qMin(left, m_editor->document()->line(line).length())));
}

void VimInputBinding::join(int count, bool spaces)
{
    if (m_editor->isReadOnly()) return;
    const int first = visual() ? qMin(m_visualAnchor.lineNumber(), m_visualCursor.lineNumber()) : m_editor->cursor().lineNumber();
    const int last = visual() ? qMax(m_visualAnchor.lineNumber(), m_visualCursor.lineNumber()) : qMin(m_editor->document()->lines() - 1, first + qMax(2, count) - 1);
    if (first == last) return;
    QString text = m_editor->document()->line(first).text();
    const int column = text.size();
    for (int line = first + 1; line <= last; ++line) {
        QString next = m_editor->document()->line(line).text();
        if (spaces) {
            int start = 0;
            while (start < next.size() && next.at(start).isSpace()) ++start;
            next.remove(0, start);
            if (!text.isEmpty() && !text.back().isSpace() && !next.isEmpty() && next.front() != ')') text += ' ';
        }
        text += next;
    }
    QDocumentCursor selection(m_editor->document(), first, 0);
    selection.moveTo(last, m_editor->document()->line(last).length(), QDocumentCursor::KeepAnchor);
    selection.beginEditBlock(); selection.insertText(text); selection.endEditBlock();
    setMode(Normal);
    finish(QDocumentCursor(m_editor->document(), first, column));
}

void VimInputBinding::addJump()
{
    QDocumentCursor cursor = m_editor->cursor(); cursor.clearSelection(); cursor.setAutoUpdated(true);
    if (m_jumps.isEmpty() || m_jumps.last() != cursor) m_jumps.append(cursor);
    if (m_jumps.size() > 100) m_jumps.removeFirst();
    m_forwardJumps.clear();
}
void VimInputBinding::jump(bool backward, int count)
{
    QList<QDocumentCursor> &from = backward ? m_jumps : m_forwardJumps;
    QList<QDocumentCursor> &to = backward ? m_forwardJumps : m_jumps;
    while (count-- > 0 && !from.isEmpty()) {
        to.append(m_editor->cursor());
        finish(from.takeLast());
    }
}

void VimInputBinding::changeNumber(int amount)
{
    if (m_editor->isReadOnly()) return;
    QDocumentCursor cursor = m_editor->cursor();
    const QString line = cursor.line().text();
    QRegularExpression pattern("-?(?:0[xX][0-9a-fA-F]+|[0-9]+)");
    auto matches = pattern.globalMatch(line);
    while (matches.hasNext()) {
        auto match = matches.next();
        if (match.capturedEnd() <= cursor.columnNumber()) continue;
        const QString old = match.captured();
        bool ok = false;
        const bool hex = old.startsWith("0x", Qt::CaseInsensitive);
        qlonglong value = old.toLongLong(&ok, hex ? 16 : 10);
        if (!ok || (amount > 0 && value > std::numeric_limits<qlonglong>::max() - amount)
            || (amount < 0 && value < std::numeric_limits<qlonglong>::min() - amount)) return;
        QString replacement = QString::number(value + amount, hex ? 16 : 10);
        if (hex) replacement.prepend("0x");
        cursor.setColumnNumber(match.capturedStart());
        cursor.setColumnNumber(match.capturedEnd(), QDocumentCursor::KeepAnchor);
        cursor.beginEditBlock(); cursor.insertText(replacement); cursor.endEditBlock();
        cursor.movePosition(1, QDocumentCursor::PreviousCharacter);
        finish(cursor);
        break;
    }
}

void VimInputBinding::substitute(const QString &expression, int first, int last)
{
    if (m_editor->isReadOnly()) { message(tr("Read-only document")); return; }
    if (expression.size() < 2) { message(tr("Expected :s/pattern/replacement/flags")); return; }
    const QChar delimiter = expression.at(1);
    QStringList parts;
    QString part;
    for (int i = 2; i < expression.size(); ++i) {
        const QChar ch = expression.at(i);
        if (ch == delimiter && !escaped(expression, i)) { parts.append(part); part.clear(); }
        else if (ch == '\\' && i + 1 < expression.size() && expression.at(i + 1) == delimiter) part += expression.at(++i);
        else part += ch;
    }
    parts.append(part);
    if (parts.size() < 2) { message(tr("Incomplete substitute command")); return; }
    const QString flags = parts.size() > 2 ? parts.at(2).trimmed() : QString();
    if (flags.contains(QRegularExpression("[^giIn]"))) { message(tr("Supported substitute flags: g i I n")); return; }
    const QString pattern = parts.at(0).isEmpty() ? m_searchPattern : parts.at(0);
    if (pattern.isEmpty()) { message(tr("No previous pattern")); return; }
    QRegularExpression::PatternOptions options;
    if ((m_ignoreCase || flags.contains('i')) && !flags.contains('I')) options |= QRegularExpression::CaseInsensitiveOption;
    QRegularExpression regex(pattern, options);
    if (!regex.isValid()) { message(regex.errorString()); return; }
    m_searchPattern = pattern;
    QDocumentCursor transaction(m_editor->document(), first, 0);
    transaction.beginEditBlock();
    int replacements = 0;
    for (int line = last; line >= first; --line) {
        const QString text = m_editor->document()->line(line).text();
        QString output;
        int end = 0;
        auto matches = regex.globalMatch(text);
        while (matches.hasNext()) {
            const auto match = matches.next();
            output += text.mid(end, match.capturedStart() - end);
            const QString replacement = parts.at(1);
            for (int i = 0; i < replacement.size(); ++i) {
                QChar ch = replacement.at(i);
                if (ch == '&') output += match.captured();
                else if (ch == '\\' && i + 1 < replacement.size()) {
                    ch = replacement.at(++i);
                    if (ch.isDigit()) output += match.captured(ch.digitValue());
                    else if (ch == 'r' || ch == 'n') output += '\n';
                    else output += ch;
                } else output += ch;
            }
            end = match.capturedEnd();
            ++replacements;
            if (!flags.contains('g')) break;
        }
        output += text.mid(end);
        if (end > 0 || output != text) {
            if (!flags.contains('n')) {
                QDocumentCursor c(m_editor->document(), line, 0);
                c.movePosition(1, QDocumentCursor::EndOfLine, QDocumentCursor::KeepAnchor);
                c.insertText(output);
            }
        }
    }
    transaction.endEditBlock();
    finish(QDocumentCursor(m_editor->document(), first, 0));
    message(tr("%1 matches").arg(replacements));
}

void VimInputBinding::executeEx(QString command)
{
    command = command.trimmed();
    if (command.startsWith(':')) command.remove(0, 1);
    int first = m_editor->cursor().lineNumber(), last = first;
    bool ranged = false;
    if (m_rangeFirst >= 0) { first = m_rangeFirst; last = m_rangeLast; ranged = true; }
    if (command.startsWith("'<,'>")) command.remove(0, 5);
    if (command.startsWith('%')) { first = 0; last = m_editor->document()->lines() - 1; command.remove(0, 1); ranged = true; }
    else {
        QRegularExpression range("^([0-9]+|\\.|\\$)(?:,([0-9]+|\\.|\\$))?");
        auto match = range.match(command);
        if (match.hasMatch()) {
            auto address = [this](const QString &s) { return s == "." ? m_editor->cursor().lineNumber() : s == "$" ? m_editor->document()->lines() - 1 : s.toInt() - 1; };
            first = address(match.captured(1));
            last = match.captured(2).isEmpty() ? first : address(match.captured(2));
            command.remove(0, match.capturedLength()); ranged = true;
        }
    }
    if (first < 0 || last < first || last >= m_editor->document()->lines()) { message(tr("Invalid line range")); return; }
    command = command.trimmed();
    if (command.isEmpty()) { addJump(); finish(QDocumentCursor(m_editor->document(), last, 0)); return; }
    int globalPrefix = command.startsWith("vglobal") ? 7 : command.startsWith("global") ? 6
        : command.size() > 1 && (command.at(0) == 'g' || command.at(0) == 'v') && !command.at(1).isLetterOrNumber() && !command.at(1).isSpace() ? 1 : 0;
    if (globalPrefix && globalPrefix < command.size()) {
        const QChar delimiter = command.at(globalPrefix);
        int end = globalPrefix + 1;
        while (end < command.size() && (command.at(end) != delimiter || escaped(command, end))) ++end;
        if (end == command.size()) { message(tr("Incomplete global command")); return; }
        QString pattern = command.mid(globalPrefix + 1, end - globalPrefix - 1);
        if (pattern.isEmpty()) pattern = m_searchPattern;
        if (pattern.isEmpty()) { message(tr("No previous pattern")); return; }
        pattern.replace(QString("\\") + delimiter, QString(delimiter));
        QRegularExpression regex(pattern, m_ignoreCase ? QRegularExpression::CaseInsensitiveOption : QRegularExpression::NoPatternOption);
        if (!regex.isValid()) { message(regex.errorString()); return; }
        const QString action = command.mid(end + 1).trimmed();
        if (!(action == "d" || action == "delete" || action == "y" || action == "yank" || action.isEmpty() || action == "p" || action == "print"
            || (action.startsWith('s') && action.size() > 1 && !action.at(1).isLetterOrNumber()))) {
            message(tr("Global supports delete, yank, print and substitute")); return;
        }
        if (!ranged) { first = 0; last = m_editor->document()->lines() - 1; }
        QList<int> selected; QStringList contents;
        for (int line = first; line <= last; ++line) {
            const QString text = m_editor->document()->line(line).text();
            if (regex.match(text).hasMatch() != command.startsWith('v')) { selected.append(line); contents.append(text); }
        }
        if (action.isEmpty() || action == "p" || action == "print") { message(contents.join(" | ").left(1000)); return; }
        if (action == "y" || action == "yank") { storeRegister(contents.join('\n') + '\n', 1, 'y'); return; }
        if (m_editor->isReadOnly()) { message(tr("Read-only document")); return; }
        m_editor->document()->beginMacro();
        for (int i = selected.size() - 1; i >= 0; --i) {
            if (action.startsWith('s')) substitute(action, selected.at(i), selected.at(i));
            else operate(lines(selected.at(i), selected.at(i)), 'd', true);
        }
        m_editor->document()->endMacro();
        if (action == "d" || action == "delete") storeRegister(contents.join('\n') + '\n', 1, 'd');
        message(tr("%1 matching lines").arg(selected.size()));
        return;
    }
    if (command.startsWith('s') && command.size() > 1 && !command.at(1).isLetter() && !command.at(1).isSpace()) {
        substitute(command, first, last); return;
    }
    const int split = command.indexOf(' ');
    const QString name = split < 0 ? command : command.left(split);
    const QString argument = split < 0 ? QString() : command.mid(split + 1).trimmed();
    if (name == "normal" || name == "normal!") {
        QList<Key> keys;
        for (int i = 0; i < argument.size(); ++i) {
            if (argument.at(i) == '<') {
                const int end = argument.indexOf('>', i + 1);
                const QString token = end >= 0 ? argument.mid(i + 1, end - i - 1).toLower() : QString();
                if (token == "esc" || token == "cr" || token == "enter" || token == "tab") {
                    keys.append({token == "esc" ? Qt::Key_Escape : token == "tab" ? Qt::Key_Tab : Qt::Key_Return, Qt::NoModifier, QString()});
                    i = end; continue;
                }
                if (token.startsWith("c-") && token.size() == 3) {
                    keys.append({token.at(2).toUpper().unicode(), Qt::ControlModifier, QString()}); i = end; continue;
                }
            }
            keys.append({argument.at(i).toUpper().unicode(), Qt::NoModifier, QString(argument.at(i))});
        }
        QList<QDocumentCursor> targets;
        for (int line = first; line <= last; ++line) {
            QDocumentCursor cursor(m_editor->document(), line, 0); cursor.setAutoUpdated(true); cursor.setAutoErasable(true); targets.append(cursor);
        }
        m_editor->document()->beginMacro();
        for (const QDocumentCursor &cursor : targets) {
            if (!cursor.isValid()) continue;
            finish(cursor); replay(keys, 1);
            if (inserting()) { QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier); QApplication::sendEvent(m_editor, &escape); }
        }
        m_editor->document()->endMacro();
        return;
    }
    if (name == "r" || name == "read") {
        if (m_editor->isReadOnly()) { message(tr("Read-only document")); return; }
        QFile file(argument);
        if (!file.open(QIODevice::ReadOnly)) { message(file.errorString()); return; }
        QString text = QString::fromUtf8(file.readAll()); text.replace("\r\n", "\n");
        if (text.endsWith('\n')) text.chop(1);
        QDocumentCursor cursor(m_editor->document(), last, m_editor->document()->line(last).length());
        cursor.beginEditBlock(); cursor.insertText("\n" + text); cursor.endEditBlock(); finish(cursor);
        return;
    }
    if (ranged && (name == "w" || name == "write" || name == "wq")) { message(tr("Writing a line range is not supported")); return; }
    if (name == "w" || name == "write" || name == "wq" || name == "x" || name == "xit" || name == "q" || name == "quit"
        || name == "q!" || name == "quit!" || name == "qa" || name == "qall" || name == "wa" || name == "wall"
        || name == "e" || name == "edit" || name == "tabnew" || name == "enew" || name == "bn" || name == "bnext"
        || name == "bp" || name == "bprevious" || name == "tabn" || name == "tabnext" || name == "tabp" || name == "tabprevious" || name == "bd" || name == "bdelete") {
        // Defer application actions: closing a tab may delete this binding.
        const QString cmd = name, arg = argument;
        QMetaObject::invokeMethod(this, [this, cmd, arg] { emit applicationCommand(cmd, arg); }, Qt::QueuedConnection);
    } else if (name == "u" || name == "undo" || name == "red" || name == "redo") {
        m_ignoreChange = true;
        if (!m_editor->isReadOnly()) { if (name == "u" || name == "undo") m_editor->undo(); else m_editor->redo(); }
        finish(m_editor->cursor());
    } else if (name == "d" || name == "delete" || name == "y" || name == "yank" || name == ">" || name == "<") {
        if (!argument.isEmpty()) m_register = argument.at(0);
        operate(lines(first, last), name.at(0), true);
    } else if (name == "j" || name == "join") {
        m_editor->setCursor(QDocumentCursor(m_editor->document(), first, 0));
        join(ranged ? last - first + 1 : 2, true);
    } else if (name == "sort" || name == "sort!") {
        if (m_editor->isReadOnly()) { message(tr("Read-only document")); return; }
        if (!ranged) { first = 0; last = m_editor->document()->lines() - 1; }
        QStringList values;
        for (int line = first; line <= last; ++line) values.append(m_editor->document()->line(line).text());
        std::sort(values.begin(), values.end());
        if (argument == "u") values.removeDuplicates();
        else if (!argument.isEmpty()) { message(tr("Supported sort option: u")); return; }
        if (name == "sort!") std::reverse(values.begin(), values.end());
        QDocumentCursor c(m_editor->document(), first, 0);
        c.moveTo(last, m_editor->document()->line(last).length(), QDocumentCursor::KeepAnchor);
        c.beginEditBlock(); c.insertText(values.join('\n')); c.endEditBlock();
        finish(QDocumentCursor(m_editor->document(), first, 0));
    } else if (name == "set" || name == "setlocal") {
        const QStringList options = argument.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        for (const QString &option : options) {
            if (option == "ic" || option == "ignorecase") m_ignoreCase = true;
            else if (option == "noic" || option == "noignorecase") m_ignoreCase = false;
            else if (option == "scs" || option == "smartcase") m_smartCase = true;
            else if (option == "noscs" || option == "nosmartcase") m_smartCase = false;
            else if (option == "wrap" || option == "nowrap") m_editor->setLineWrapping(option == "wrap");
            else if (option == "ai" || option == "autoindent" || option == "noai" || option == "noautoindent") m_editor->setFlag(QEditor::AutoIndent, !option.startsWith("no"));
            else if (option == "et" || option == "expandtab" || option == "noet" || option == "noexpandtab") m_editor->setFlag(QEditor::ReplaceIndentTabs, !option.startsWith("no"));
            else if (option.startsWith("ts=") || option.startsWith("tabstop=")) {
                bool ok = false; int width = option.section('=', 1).toInt(&ok);
                if (ok && width > 0 && width <= 32) QDocument::setTabStop(width);
                else { message(tr("tabstop must be between 1 and 32")); return; }
            } else { message(tr("Unsupported option: %1").arg(option)); return; }
        }
        message(options.isEmpty() ? tr("Options: ignorecase smartcase wrap autoindent expandtab tabstop") : tr("Options updated for this session"));
    } else if (name == "noh" || name == "nohlsearch") m_commandLine->hide();
    else if (name == "reg" || name == "registers") {
        QStringList registers;
        for (auto it = m_registers.cbegin(); it != m_registers.cend(); ++it) registers.append(QString(it.key()) + ": " + it.value().text.left(30).replace('\n', "^J"));
        message(registers.join("  "));
    } else if (name == "marks") {
        QStringList marks;
        for (auto it = m_marks.cbegin(); it != m_marks.cend(); ++it) marks.append(QString(it.key()) + ":" + QString::number(it.value().lineNumber() + 1));
        message(marks.join("  "));
    } else if (name == "help") message(tr("Vim command reference: VIM_MODE.md beside texstudio.exe"));
    else message(tr("Unsupported Ex command: %1").arg(name));
}
