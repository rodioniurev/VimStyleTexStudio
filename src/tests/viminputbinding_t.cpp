// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 rodioniurev
// Optional Vim editing for TeXstudio, added and modified 2026-10-24.

#ifndef QT_NO_DEBUG
#include "viminputbinding_t.h"
#include "viminputbinding.h"
#include "qeditor.h"
#include "qdocumentline.h"
#include "qformatscheme.h"
#include <QtTest/QtTest>
#include <QClipboard>
#include <QInputMethodEvent>
#include <QFocusEvent>
#include <QMimeData>
#include <QLineEdit>
#include <QtTest/QSignalSpy>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>

VimInputBindingTest::VimInputBindingTest() = default;
VimInputBindingTest::~VimInputBindingTest() = default;

void VimInputBindingTest::initTestCase()
{
    definitionsFile = qApp->property("vimDefinitionsFile");
    qApp->setProperty("vimDefinitionsFile", QString());
    clipboard.reset(new QMimeData);
    const QMimeData *original = QApplication::clipboard()->mimeData();
    if (original) for (const QString &format : original->formats()) clipboard->setData(format, original->data(format));
}
void VimInputBindingTest::cleanupTestCase()
{
    QApplication::clipboard()->setMimeData(clipboard.release());
    qApp->setProperty("vimDefinitionsFile", definitionsFile);
    QEditor unused; VimInputBinding restore(&unused);
}

void VimInputBindingTest::init()
{
    tabStop = QDocument::tabStop();
    editor.reset(new QEditor);
    editor->document()->setFormatScheme(new QFormatScheme(editor->document()));
    editor->setFlag(QEditor::AutoCloseChars, false);
    editor->setFlag(QEditor::AutoIndent, false);
    editor->setText("one two three\nfour five\nsix", false);
    editor->setCursorPosition(0, 0);
    vim = new VimInputBinding(editor.get());
    editor->setInputBinding(vim);
    vim->setEnabled(true);
    ex("abclear"); ex("mapclear"); ex("mapclear!");
}

void VimInputBindingTest::cleanup() { editor.reset(); QDocument::setTabStop(tabStop); }
void VimInputBindingTest::keys(const char *text) { QTest::keyClicks(editor.get(), text); }
void VimInputBindingTest::escape() { QTest::keyClick(editor.get(), Qt::Key_Escape); }
void VimInputBindingTest::ex(const QString &text) { QTest::keyClicks(editor.get(), ":" + text); QTest::keyClick(editor.get(), Qt::Key_Return); }

void VimInputBindingTest::modesAndDisabledInput()
{
    QCOMPARE(editor->property("vimMode").toString(), QString("VIM NORMAL"));
    keys("iHello ");
    QVERIFY(editor->text().startsWith("Hello one"));
    QTest::keyClick(editor.get(), Qt::Key_Escape);
    QCOMPARE(editor->property("vimMode").toString(), QString("VIM NORMAL"));
    QCOMPARE(editor->cursor().columnNumber(), 5);
    keys("A!");
    QTest::keyClick(editor.get(), Qt::Key_BracketLeft, Qt::ControlModifier);
    QVERIFY(editor->text().startsWith("Hello one two three!"));
    keys("oNext");
    QTest::keyClick(editor.get(), Qt::Key_Escape);
    QCOMPARE(editor->document()->line(1).text(), QString("Next"));
    vim->setEnabled(false);
    QVERIFY(editor->property("vimMode").toString().isEmpty());
    keys("Z");
    QCOMPARE(editor->document()->line(1).text(), QString("NexZt"));
}

void VimInputBindingTest::motions()
{
    keys("2w"); QCOMPARE(editor->cursor().columnNumber(), 8);
    keys("b"); QCOMPARE(editor->cursor().columnNumber(), 4);
    keys("e"); QCOMPARE(editor->cursor().columnNumber(), 6);
    keys("0"); QCOMPARE(editor->cursor().columnNumber(), 0);
    keys("99l"); QCOMPARE(editor->cursor().columnNumber(), 12);
    keys("2gg"); QCOMPARE(editor->cursor().lineNumber(), 1);
    keys("G"); QCOMPARE(editor->cursor().lineNumber(), 2);
    keys("gg"); QCOMPARE(editor->cursor().lineNumber(), 0);
    keys("j$"); QCOMPARE(editor->cursor().columnNumber(), 8);
}

void VimInputBindingTest::operators_data()
{
    QTest::addColumn<QString>("command");
    QTest::addColumn<QString>("expected");
    QTest::newRow("word") << "dw" << "two three\nfour five\nsix";
    QTest::newRow("end-word-inclusive") << "de" << " two three\nfour five\nsix";
    QTest::newRow("counts") << "2d2w" << "five\nsix";
    QTest::newRow("line") << "dd" << "four five\nsix";
    QTest::newRow("lines") << "2dd" << "six";
    QTest::newRow("last-line") << "Gdd" << "one two three\nfour five";
    QTest::newRow("all-lines") << "dG" << "";
    QTest::newRow("characters") << "3x" << " two three\nfour five\nsix";
    QTest::newRow("end-line") << "D" << "\nfour five\nsix";
    QTest::newRow("change-line") << "ccNEW" << "NEW\nfour five\nsix";
    QTest::newRow("change-word") << "cwNEW" << "NEW two three\nfour five\nsix";
    QTest::newRow("change-last-line") << "GccNEW" << "one two three\nfour five\nNEW";
    QTest::newRow("invalid") << "dz" << "one two three\nfour five\nsix";
}

void VimInputBindingTest::operators()
{
    QFETCH(QString, command); QFETCH(QString, expected);
    QTest::keyClicks(editor.get(), command);
    QCOMPARE(editor->text(), expected);
}

void VimInputBindingTest::linewiseClipboard()
{
    keys("yyGp");
    QCOMPARE(editor->text(), QString("one two three\nfour five\nsix\none two three"));
    keys("ggddP");
    QCOMPARE(editor->text(), QString("one two three\nfour five\nsix\none two three"));
    QApplication::clipboard()->setText("ABC");
    keys("p");
    QCOMPARE(editor->document()->line(0).text(), QString("oABCne two three"));
}

void VimInputBindingTest::visualSelection()
{
    keys("vll");
    QCOMPARE(editor->cursor().selectedText(), QString("one"));
    keys("d");
    QCOMPARE(editor->document()->line(0).text(), QString(" two three"));
    keys("llvhh");
    QCOMPARE(editor->cursor().selectedText(), QString(" tw"));
    QTest::keyClick(editor.get(), Qt::Key_Escape);
    keys("Vjd");
    QCOMPARE(editor->text(), QString("six"));
}

void VimInputBindingTest::undoAndRedo()
{
    const QString original = editor->text();
    keys("2dd"); QCOMPARE(editor->text(), QString("six"));
    keys("u"); QCOMPARE(editor->text(), original);
    QTest::keyClick(editor.get(), Qt::Key_R, Qt::ControlModifier);
    QCOMPARE(editor->text(), QString("six"));
}

void VimInputBindingTest::readOnly()
{
    const QString original = editor->text();
    editor->document()->setReadOnly(true);
    keys("ddxDccipPu");
    QTest::keyClick(editor.get(), Qt::Key_R, Qt::ControlModifier);
    QCOMPARE(editor->text(), original);
    keys("yy");
    QCOMPARE(QApplication::clipboard()->text(), QString("one two three\n"));
}

void VimInputBindingTest::perEditorState()
{
    QEditor other;
    other.document()->setFormatScheme(new QFormatScheme(other.document()));
    other.setText("other", false);
    VimInputBinding otherVim(&other);
    other.setInputBinding(&otherVim);
    otherVim.setEnabled(true);
    keys("d");
    QTest::keyClicks(&other, "iX");
    QCOMPARE(other.text(), QString("Xother"));
    keys("w");
    QVERIFY(editor->text().startsWith("two three"));
    QCOMPARE(other.property("vimMode").toString(), QString("VIM INSERT"));
}

void VimInputBindingTest::shortcutsAndInputMethod()
{
    QKeyEvent shortcut(QEvent::ShortcutOverride, Qt::Key_R, Qt::ControlModifier);
    shortcut.ignore();
    QApplication::sendEvent(editor.get(), &shortcut);
    QVERIFY(shortcut.isAccepted());
    QKeyEvent buildShortcut(QEvent::ShortcutOverride, Qt::Key_F5, Qt::NoModifier);
    buildShortcut.ignore(); QApplication::sendEvent(editor.get(), &buildShortcut);
    QVERIFY(!buildShortcut.isAccepted());
    const QString original = editor->text();
    QInputMethodEvent commit;
    commit.setCommitString(QString::fromUtf8("текст"));
    QApplication::sendEvent(editor.get(), &commit);
    QCOMPARE(editor->text(), original);
    keys("i");
    QApplication::sendEvent(editor.get(), &commit);
    QVERIFY(editor->text().startsWith(QString::fromUtf8("текст")));
    QTest::keyClick(editor.get(), Qt::Key_Escape);
    keys("d"); QTest::keyClick(editor.get(), Qt::Key_Escape); keys("x");
    QVERIFY(editor->text().startsWith(QString::fromUtf8("тексone")));
    editor->setText("one", false); editor->setCursorPosition(0, 0); keys("2i");
    QInputMethodEvent unicode; unicode.setCommitString(QString::fromUtf8("т"));
    QApplication::sendEvent(editor.get(), &unicode); escape();
    QCOMPARE(editor->text(), QString::fromUtf8("ттone"));
}
void VimInputBindingTest::objects_data()
{
    QTest::addColumn<QString>("text"); QTest::addColumn<int>("column");
    QTest::addColumn<QString>("command"); QTest::addColumn<QString>("expected");
    QTest::newRow("inner-word") << "one two" << 1 << "ciwX" << "X two";
    QTest::newRow("around-word") << "one two" << 1 << "daw" << "two";
    QTest::newRow("around-last-word") << "one two" << 5 << "daw" << "one";
    QTest::newRow("inner-parens") << "call(foo, (bar)) rest" << 12 << "di(" << "call(foo, ()) rest";
    QTest::newRow("outer-parens") << "call(foo) rest" << 6 << "da)" << "call rest";
    QTest::newRow("nested-parens") << "(a(b)c)" << 3 << "d2i(" << "()";
    QTest::newRow("tex-braces") << "\\cmd{hello}" << 7 << "ciBX" << "\\cmd{X}";
    QTest::newRow("double-quotes") << "say \"hello\"." << 6 << "ci\"X" << "say \"X\".";
    QTest::newRow("single-quotes") << "say 'hello'." << 6 << "da'" << "say.";
    QTest::newRow("backticks") << "`hello`" << 2 << "di`" << "``";
    QTest::newRow("square-brackets") << "[hello]" << 2 << "di]" << "[]";
    QTest::newRow("uppercase-object") << "one two" << 0 << "gUiw" << "ONE two";
    QTest::newRow("lowercase-motion") << "ONE TWO" << 0 << "guw" << "one TWO";
    QTest::newRow("missing-object") << "one two" << 1 << "di{" << "one two";
}
void VimInputBindingTest::objects()
{
    QFETCH(QString, text); QFETCH(int, column); QFETCH(QString, command); QFETCH(QString, expected);
    editor->setText(text, false); editor->setCursorPosition(0, column);
    QTest::keyClicks(editor.get(), command); QCOMPARE(editor->text(), expected);
    escape();
}

void VimInputBindingTest::objectOracle_data()
{
    QTest::addColumn<QString>("text"); QTest::addColumn<int>("line"); QTest::addColumn<int>("column");
    QTest::addColumn<QString>("command"); QTest::addColumn<QString>("expected"); QTest::addColumn<QString>("reg"); QTest::addColumn<bool>("insert"); QTest::addColumn<bool>("windowsEnding");
    QFile file(QFINDTESTDATA("vim_object_oracle.json"));
    QVERIFY(file.open(QIODevice::ReadOnly));
    for (const QJsonValue &value : QJsonDocument::fromJson(file.readAll()).array()) {
        const QJsonObject row = value.toObject();
        for (bool windowsEnding : {false, true}) QTest::newRow((row["name"].toString() + (windowsEnding ? "-CRLF" : "-LF")).toUtf8().constData())
            << row["text"].toString() << row["line"].toInt() << row["column"].toInt()
            << row["command"].toString() << row["expected"].toString() << row["register"].toString() << (row["insert"].toInt() != 0) << windowsEnding;
    }
}

void VimInputBindingTest::objectOracle()
{
    QFETCH(QString, text); QFETCH(int, line); QFETCH(int, column); QFETCH(QString, command);
    QFETCH(QString, expected); QFETCH(QString, reg); QFETCH(bool, insert); QFETCH(bool, windowsEnding);
    editor->setText(text, false); editor->setCursorPosition(line, column);
    editor->document()->setLineEndingDirect(windowsEnding ? QDocument::Windows : QDocument::Unix, true);
    QApplication::clipboard()->setText(QString());
    QTest::keyClicks(editor.get(), command);
    QCOMPARE(editor->property("vimMode").toString() == "VIM INSERT", insert);
    if (insert) { keys("X"); escape(); }
    QCOMPARE(editor->document()->textLines().join('\n'), expected);
    QCOMPARE(editor->document()->lineEnding(), windowsEnding ? QDocument::Windows : QDocument::Unix);
    if (command.startsWith('y')) QCOMPARE(QApplication::clipboard()->text(), reg);
}

void VimInputBindingTest::objectWorkflow()
{
    const QString original = "one two three\nfour five";
    editor->setText(original, false); editor->setCursorPosition(0, 1);
    keys("ciwX"); escape(); keys("u");
    QCOMPARE(editor->text(), original);
    editor->setCursorPosition(0, 1);
    keys("ciwX"); escape(); keys("w.");
    QCOMPARE(editor->document()->line(0).text(), QString("X X three"));
    editor->setText(original, false); editor->setCursorPosition(0, 1);
    keys("\"aciwX"); escape(); keys("\"aP");
    QCOMPARE(editor->document()->line(0).text(), QString("oneX two three"));
    editor->setText(original, false); editor->setCursorPosition(0, 1);
    keys("c"); escape(); keys("aw"); escape();
    QCOMPARE(editor->document()->line(0).text(), QString("onwe two three"));
    editor->setText(original, false); editor->setCursorPosition(0, 1);
    editor->document()->setReadOnly(true);
    keys("ciwcawdaw");
    QCOMPARE(editor->text(), original);
    QCOMPARE(editor->property("vimMode").toString(), QString("VIM NORMAL"));
}

void VimInputBindingTest::searching()
{
    editor->setText("one two one TWO", false); editor->setCursorPosition(0, 0);
    keys("/one"); QTest::keyClick(editor.get(), Qt::Key_Return); QCOMPARE(editor->cursor().columnNumber(), 8);
    keys("n"); QCOMPARE(editor->cursor().columnNumber(), 0);
    keys("N"); QCOMPARE(editor->cursor().columnNumber(), 8);
    keys("?two"); QTest::keyClick(editor.get(), Qt::Key_Return); QCOMPARE(editor->cursor().columnNumber(), 4);
    ex("set ignorecase smartcase");
    keys("/two"); QTest::keyClick(editor.get(), Qt::Key_Return); QCOMPARE(editor->cursor().columnNumber(), 12);
    keys("/TWO"); QTest::keyClick(editor.get(), Qt::Key_Return); QCOMPARE(editor->cursor().columnNumber(), 12);
    keys("0*"); QCOMPARE(editor->cursor().columnNumber(), 8);
    keys("#"); QCOMPARE(editor->cursor().columnNumber(), 0);
    keys("d/two"); QTest::keyClick(editor.get(), Qt::Key_Return);
    QCOMPARE(editor->text(), QString("two one TWO"));
}

void VimInputBindingTest::exCommands_data()
{
    QTest::addColumn<QString>("text"); QTest::addColumn<QString>("command"); QTest::addColumn<QString>("expected");
    QTest::newRow("substitute-first") << "one one\none one" << "s/one/X/" << "X one\none one";
    QTest::newRow("substitute-all") << "one one\none one" << "%s/one/X/g" << "X X\nX X";
    QTest::newRow("substitute-range") << "one\none\none" << "2,3s/one/X/" << "one\nX\nX";
    QTest::newRow("substitute-captures") << "abc12" << "s/([a-z]+)([0-9]+)/\\2-\\1/" << "12-abc";
    QTest::newRow("substitute-ampersand") << "one" << "s/one/[&]/" << "[one]";
    QTest::newRow("substitute-count") << "one one" << "%s/one/X/gn" << "one one";
    QTest::newRow("substitute-delimiter") << "a/b" << "s#a/b#x#" << "x";
    QTest::newRow("substitute-ignorecase") << "ONE one" << "%s/one/X/gi" << "X X";
    QTest::newRow("range-delete") << "a\nb\nc\nd" << "2,3d" << "a\nd";
    QTest::newRow("last-line-delete") << "a\nb\nc" << "$d" << "a\nb";
    QTest::newRow("sort") << "c\na\nb" << "%sort" << "a\nb\nc";
    QTest::newRow("sort-reverse") << "a\nb\nc" << "%sort!" << "c\nb\na";
    QTest::newRow("sort-unique") << "c\na\na" << "%sort u" << "a\nc";
    QTest::newRow("join") << "one\n  two\nthree" << "1,3join" << "one two three";
    QTest::newRow("unknown-command") << "one" << "notacommand" << "one";
    QTest::newRow("invalid-regex") << "one" << "%s/[/X/g" << "one";
    QTest::newRow("invalid-range") << "one" << "2,4d" << "one";
    QTest::newRow("unsupported-flag") << "one" << "%s/one/X/c" << "one";
    QTest::newRow("global-delete") << "one\ntwo\none\nthree" << "g/one/d" << "two\nthree";
    QTest::newRow("inverse-global") << "one\ntwo\none\nthree" << "v/one/d" << "one\none";
    QTest::newRow("global-substitute") << "one\ntwo\none" << "g/one/s/o/O/" << "One\ntwo\nOne";
    QTest::newRow("global-yank") << "one\ntwo\none" << "g/one/y" << "one\ntwo\none";
    QTest::newRow("normal-range") << "one\ntwo\nthree" << "%normal I><Esc>" << ">one\n>two\n>three";
    QTest::newRow("normal-count") << "one two three" << "normal 2dw" << "three";
    QTest::newRow("reject-range-write") << "one\ntwo" << "1,2w anything.tex" << "one\ntwo";
}
void VimInputBindingTest::exCommands()
{
    QFETCH(QString, text); QFETCH(QString, command); QFETCH(QString, expected);
    editor->setText(text, false); editor->setCursorPosition(0, 0);
    ex(command); QCOMPARE(editor->text(), expected);
}

void VimInputBindingTest::commandHistoryAndErrors()
{
    keys("/two"); escape(); QCOMPARE(editor->cursor().columnNumber(), 0);
    QVERIFY(editor->findChild<QLineEdit *>("vimCommandLine")->isHidden());
    keys("/two"); QTest::keyClick(editor.get(), Qt::Key_Return);
    keys("/"); QTest::keyClick(editor.get(), Qt::Key_Up); QTest::keyClick(editor.get(), Qt::Key_Return);
    QCOMPARE(editor->cursor().columnNumber(), 4);
    ex("bogus"); QVERIFY(editor->property("vimMessage").toString().contains("Unsupported"));
    ex("set ts=0"); QVERIFY(editor->property("vimMessage").toString().contains("tabstop"));
    ex("set ts=3 expandtab"); QCOMPARE(QDocument::tabStop(), 3);
    keys("gg>>"); QVERIFY(editor->text().startsWith("   one"));
}

void VimInputBindingTest::repeatAndMacros()
{
    keys("dw."); QCOMPARE(editor->document()->line(0).text(), QString("three"));
    editor->setText("one two three", false); editor->setCursorPosition(0, 0);
    keys("cwX"); escape(); keys("w."); QCOMPARE(editor->text(), QString("X X three"));
    keys("u"); QCOMPARE(editor->text(), QString("X two three"));
    editor->setText("one two three", false); editor->setCursorPosition(0, 0);
    keys("qaxlq@a"); QCOMPARE(editor->text(), QString("n two three"));
    keys("@@"); QCOMPARE(editor->text(), QString("n wo three"));
    editor->setText("aaaaaaaaaaaaaaaaaaaa", false); editor->setCursorPosition(0, 0);
    keys("qbx@bq@b"); QVERIFY(editor->text().size() < 20); // bounded recursion, no hang
    editor->setText("one", false); editor->setCursorPosition(0, 0);
    keys("3iX"); escape(); QCOMPARE(editor->text(), QString("XXXone"));
    keys("u"); QCOMPARE(editor->text(), QString("one"));
    keys("qc3iX"); escape(); keys("qu@c"); QCOMPARE(editor->text(), QString("XXXone"));
}

void VimInputBindingTest::namedRegisters()
{
    keys("\"ayyj\"ap"); QCOMPARE(editor->document()->line(2).text(), QString("one two three"));
    QApplication::clipboard()->setText("KEEP");
    keys("\"_dd"); QCOMPARE(QApplication::clipboard()->text(), QString("KEEP"));
    keys("gg\"bywG\"bP"); QVERIFY(editor->document()->line(editor->document()->lines() - 1).text().startsWith("one "));
    QEditor other; other.document()->setFormatScheme(new QFormatScheme(other.document()));
    other.setText("Z", false); other.setCursorPosition(0, 0);
    VimInputBinding otherVim(&other); other.setInputBinding(&otherVim); otherVim.setEnabled(true);
    QTest::keyClicks(&other, "\"aP"); QVERIFY(other.text().startsWith("one two three\n"));
}

void VimInputBindingTest::characterFind()
{
    editor->setText("a:b:c:d", false); editor->setCursorPosition(0, 0);
    keys("f:"); QCOMPARE(editor->cursor().columnNumber(), 1);
    keys(";"); QCOMPARE(editor->cursor().columnNumber(), 3);
    keys(","); QCOMPARE(editor->cursor().columnNumber(), 1);
    keys("0dt:"); QCOMPARE(editor->text(), QString(":b:c:d"));
    editor->setText("a:b:c:d", false); editor->setCursorPosition(0, 5);
    keys("dF:"); QCOMPARE(editor->text(), QString("a:bd"));
    editor->setText("ab:cd:ef", false); editor->setCursorPosition(0, 0);
    keys("t:"); QCOMPARE(editor->cursor().columnNumber(), 1);
    keys(";"); QCOMPARE(editor->cursor().columnNumber(), 4);
    keys("0F:"); QCOMPARE(editor->cursor().columnNumber(), 0);
}

void VimInputBindingTest::marksAndJumps()
{
    keys("maG`a"); QCOMPARE(editor->cursor().lineNumber(), 0);
    QTest::keyClick(editor.get(), Qt::Key_O, Qt::ControlModifier); QCOMPARE(editor->cursor().lineNumber(), 2);
    QTest::keyClick(editor.get(), Qt::Key_I, Qt::ControlModifier); QCOMPARE(editor->cursor().lineNumber(), 0);
    keys("2wmb0`b"); QCOMPARE(editor->cursor().columnNumber(), 8);
    keys("'b"); QCOMPARE(editor->cursor().columnNumber(), 0);
    keys("viw"); escape(); keys("gv"); QCOMPARE(editor->cursor().selectedText(), QString("one")); escape();
}

void VimInputBindingTest::extraEditing_data()
{
    QTest::addColumn<QString>("text"); QTest::addColumn<QString>("command"); QTest::addColumn<QString>("expected");
    QTest::newRow("replace-char") << "one two" << "rX" << "Xne two";
    QTest::newRow("replace-count") << "one two" << "3rX" << "XXX two";
    QTest::newRow("delete-backward") << "one two" << "llX" << "oe two";
    QTest::newRow("toggle-case") << "One two" << "3~" << "oNE two";
    QTest::newRow("uppercase-line") << "one two" << "gUU" << "ONE TWO";
    QTest::newRow("join-spaces") << "one\n  two" << "J" << "one two";
    QTest::newRow("join-raw") << "one\n  two" << "gJ" << "one  two";
    QTest::newRow("indent-unindent") << "one\ntwo" << ">><<" << "one\ntwo";
    QTest::newRow("big-word") << "one-two three" << "dW" << "three";
    QTest::newRow("matching-pair") << "(one) two" << "d%" << " two";
    QTest::newRow("visual-case") << "one two" << "viwU" << "ONE two";
}
void VimInputBindingTest::extraEditing()
{
    QFETCH(QString, text); QFETCH(QString, command); QFETCH(QString, expected);
    editor->setText(text, false); editor->setCursorPosition(0, 0);
    QTest::keyClicks(editor.get(), command); QCOMPARE(editor->text(), expected);
}

void VimInputBindingTest::replaceAndNumbers()
{
    keys("RXYZ"); escape(); QVERIFY(editor->text().startsWith("XYZ two"));
    editor->setText("value 12", false); editor->setCursorPosition(0, 0);
    keys("3"); QTest::keyClick(editor.get(), Qt::Key_A, Qt::ControlModifier); QCOMPARE(editor->text(), QString("value 15"));
    QTest::keyClick(editor.get(), Qt::Key_X, Qt::ControlModifier); QCOMPARE(editor->text(), QString("value 14"));
}

void VimInputBindingTest::blockEditing()
{
    editor->setText("abc\ndef\nghi", false); editor->setCursorPosition(0, 0);
    QTest::keyClick(editor.get(), Qt::Key_V, Qt::ControlModifier); keys("ljd");
    QCOMPARE(editor->text(), QString("c\nf\nghi"));
    keys("P"); QCOMPARE(editor->text(), QString("abc\ndef\nghi"));
    keys("0"); QTest::keyClick(editor.get(), Qt::Key_V, Qt::ControlModifier); keys("ljcX"); escape();
    QCOMPARE(editor->text(), QString("Xc\nXf\nghi"));
    editor->setText("abc\ndef\nghi", false); editor->setCursorPosition(0, 0);
    QApplication::clipboard()->setText(QString());
    QTest::keyClick(editor.get(), Qt::Key_V, Qt::ControlModifier); keys("ljp"); escape();
    QCOMPARE(editor->text(), QString("abc\ndef\nghi"));
    editor->setCursorPosition(0, 0);
    QApplication::clipboard()->setText("Z");
    QTest::keyClick(editor.get(), Qt::Key_V, Qt::ControlModifier); keys("ljp");
    QCOMPARE(editor->text(), QString("Zc\nZf\nghi"));
    keys("u"); QCOMPARE(editor->text(), QString("abc\ndef\nghi"));
    editor->setCursorPosition(0, 0);
    QTest::keyClick(editor.get(), Qt::Key_V, Qt::ControlModifier); keys("ljU");
    QCOMPARE(editor->text(), QString("ABc\nDEf\nghi"));
}

void VimInputBindingTest::extendedReadOnly()
{
    const QString original = editor->text(); editor->document()->setReadOnly(true);
    keys("rXRxxx"); escape(); keys("J>>gUiw");
    ex("%s/one/X/g"); ex("sort!");
    QTest::keyClick(editor.get(), Qt::Key_A, Qt::ControlModifier);
    QCOMPARE(editor->text(), original);
}

void VimInputBindingTest::applicationCommands()
{
    QSignalSpy spy(vim, &VimInputBinding::applicationCommand);
    ex("w example.tex"); QApplication::processEvents();
    QCOMPARE(spy.count(), 1); QCOMPARE(spy.at(0).at(0).toString(), QString("w"));
    QCOMPARE(spy.at(0).at(1).toString(), QString("example.tex"));
    ex("q!"); QApplication::processEvents(); QCOMPARE(spy.count(), 2);
    keys("gt"); QApplication::processEvents(); QCOMPARE(spy.count(), 3);
}

void VimInputBindingTest::insertControlsAndUndo()
{
    keys("ihello world"); QTest::keyClick(editor.get(), Qt::Key_W, Qt::ControlModifier);
    QVERIFY(editor->text().startsWith("hello one"));
    QTest::keyClick(editor.get(), Qt::Key_U, Qt::ControlModifier); escape();
    QCOMPARE(editor->text(), QString("one two three\nfour five\nsix"));
    keys("cwNEW"); escape(); keys("u");
    QCOMPARE(editor->text(), QString("one two three\nfour five\nsix"));
    editor->setCursorPosition(0, 0);
    keys("ihello"); escape(); keys("\".p");
    QCOMPARE(editor->document()->line(0).text(), QString("hellohelloone two three"));
}
void VimInputBindingTest::changeBlockSaveBoundaries()
{
    editor->setText("old", false);
    QDocument *doc = editor->document(); doc->setClean();
    doc->beginMacro(); doc->endMacro();
    QVERIFY(doc->isClean()); QVERIFY(!doc->canUndo());
    QSignalSpy changed(doc, SIGNAL(cleanChanged(bool)));
    doc->beginMacro(); doc->beginMacro();
    QDocumentCursor cursor(doc, 0, 0); cursor.insertText("X");
    QVERIFY(!doc->isClean()); QVERIFY(!changed.isEmpty());
    QCOMPARE(changed.last().at(0).toBool(), false);
    doc->setClean(); QVERIFY(doc->isClean());
    cursor.insertText("Y"); QVERIFY(!doc->isClean());
    doc->endMacro(); doc->endMacro();
    QCOMPARE(editor->text(), QString("XYold"));
    editor->undo(); QCOMPARE(editor->text(), QString("Xold")); QVERIFY(doc->isClean());
    editor->redo(); QCOMPARE(editor->text(), QString("XYold")); QVERIFY(!doc->isClean());
    editor->undo(); editor->undo(); QCOMPARE(editor->text(), QString("old"));
    QVERIFY(!doc->isClean());
    doc->setClean(); doc->beginMacro(); doc->setClean(); doc->endMacro();
    QVERIFY(doc->isClean());
}

void VimInputBindingTest::saveDuringInsert_data()
{
    QTest::addColumn<QString>("command"); QTest::addColumn<QString>("expected");
    QTest::newRow("insert") << "iNEW" << "NEWold value";
    QTest::newRow("append") << "aNEW" << "oNEWld value";
    QTest::newRow("replace") << "RNEW" << "NEW value";
    QTest::newRow("change-word") << "ciwNEW" << "NEW value";
}

void VimInputBindingTest::saveDuringInsert()
{
    QFETCH(QString, command); QFETCH(QString, expected);
    QTemporaryDir directory; QVERIFY(directory.isValid());
    const QString path = directory.filePath("compile-input.tex");
    editor->setText("old value", false); editor->setCursorPosition(0, 0);
    editor->save(path); QVERIFY(!editor->isContentModified());
    QTest::keyClicks(editor.get(), command);
    QCOMPARE(editor->text(), expected);
    // Match TeXstudio::fileSaveAll's gate, including F5 while still in Insert.
    QVERIFY(editor->isContentModified());
    if (editor->isContentModified()) editor->save();
    QFile saved(path); QVERIFY(saved.open(QIODevice::ReadOnly));
    QCOMPARE(QString::fromUtf8(saved.readAll()), expected); saved.close();
    QVERIFY(!editor->isContentModified());
    keys("MORE"); QVERIFY(editor->isContentModified());
    escape(); keys("u");
    QCOMPARE(editor->text(), expected);
    QVERIFY(!editor->isContentModified());
    keys("u"); QCOMPARE(editor->text(), QString("old value"));
}

void VimInputBindingTest::abbreviations_data()
{
    QTest::addColumn<QString>("definition"); QTest::addColumn<QString>("input"); QTest::addColumn<QString>("expected");
    QTest::newRow("short-ab") << "ab teh the" << "teh " << "the ";
    QTest::newRow("long-ab") << "abbreviate teh the" << "teh." << "the.";
    QTest::newRow("insert-only") << "iabbrev teh the" << "teh," << "the,";
    QTest::newRow("noremap") << "inoreabbrev teh the" << "teh " << "the ";
    QTest::newRow("word-suffix") << "iabbrev teh the" << "ateh " << "ateh ";
    QTest::newRow("word-prefix") << "iabbrev teh the" << "tehx " << "tehx ";
    QTest::newRow("punctuation-before") << "iabbrev teh the" << "(teh)" << "(the)";
    QTest::newRow("end-id") << "iabbrev #i INCLUDE" << "#i " << "INCLUDE ";
    QTest::newRow("end-id-boundary") << "iabbrev #i INCLUDE" << ">#i " << ">#i ";
    QTest::newRow("non-id") << "iabbrev ;; END" << ";; " << "END ";
    QTest::newRow("non-id-boundary") << "iabbrev ;; END" << "x;; " << "x;; ";
    QTest::newRow("rhs-spaces") << "iabbrev btw by the way" << "btw " << "by the way ";
    QTest::newRow("latex") << "iabbrev qq \\qquad" << "qq " << "\\qquad ";
    QTest::newRow("non-recursive") << "iabbrev foo foo!" << "foo " << "foo! ";
    QTest::newRow("key-notation") << "iabbrev foo one<CR>two" << "foo " << "one\ntwo ";
}
void VimInputBindingTest::abbreviations()
{
    QFETCH(QString, definition); QFETCH(QString, input); QFETCH(QString, expected);
    ex(definition); editor->setText("", false); editor->setCursorPosition(0, 0);
    keys("i"); QTest::keyClicks(editor.get(), input); escape();
    QCOMPARE(editor->text(), expected);
    keys("u"); QCOMPARE(editor->text(), QString());
}
void VimInputBindingTest::abbreviationModesAndRemoval()
{
    ex("iabbrev teh the"); editor->setText("", false); keys("iteh");
    escape(); QCOMPARE(editor->text(), QString("the"));
    editor->setText("", false); keys("iteh"); QTest::keyClick(editor.get(), Qt::Key_C, Qt::ControlModifier);
    QCOMPARE(editor->text(), QString("teh"));
    editor->setText("", false); keys("iteh"); QTest::keyClick(editor.get(), Qt::Key_BracketRight, Qt::ControlModifier);
    QCOMPARE(editor->text(), QString("the")); escape();
    editor->setText("", false); keys("iteh"); QTest::keyClick(editor.get(), Qt::Key_V, Qt::ControlModifier); keys(" ");
    QTest::keyClick(editor.get(), Qt::Key_C, Qt::ControlModifier); QCOMPARE(editor->text(), QString("teh "));
    ex("iunabbrev teh"); editor->setText("", false); keys("iteh "); escape(); QCOMPARE(editor->text(), QString("teh "));
    ex("cabbrev SS set"); ex("SS ts=6"); QCOMPARE(QDocument::tabStop(), 6);
    ex("cunabbrev SS"); ex("SS ts=7"); QCOMPARE(QDocument::tabStop(), 6);
    ex("iabbrev <buffer> zz LOCAL"); ex("iabbrev zz GLOBAL");
    editor->setText("", false); keys("izz "); escape(); QCOMPARE(editor->text(), QString("LOCAL "));
    QEditor other; other.document()->setFormatScheme(new QFormatScheme(other.document())); other.setText("", false);
    VimInputBinding binding(&other); other.setInputBinding(&binding); binding.setEnabled(true);
    QTest::keyClicks(&other, "izz "); QTest::keyClick(&other, Qt::Key_Escape); QCOMPARE(other.text(), QString("GLOBAL "));
    ex("iabclear <buffer>"); editor->setText("", false); keys("izz "); escape(); QCOMPARE(editor->text(), QString("GLOBAL "));
    ex("iabbrev <expr> bad 1+1"); QVERIFY(editor->property("vimMessage").toString().contains("Unsupported"));
    ex("iabbrev a.b INVALID"); QVERIFY(editor->property("vimMessage").toString().contains("Invalid"));
}
void VimInputBindingTest::mappings()
{
    ex("nnoremap Q dw"); keys("Q"); QCOMPARE(editor->document()->line(0).text(), QString("two three"));
    ex("nnoremap X Q"); keys("X"); QCOMPARE(editor->document()->line(0).text(), QString("two three"));
    ex("nmap X Q"); keys("X"); QCOMPARE(editor->document()->line(0).text(), QString("three"));
    ex("nunmap Q"); keys("Q"); QCOMPARE(editor->document()->line(0).text(), QString("three"));
    ex("inoremap jj <Esc>"); keys("Ajj"); QCOMPARE(editor->property("vimMode").toString(), QString("VIM NORMAL"));
    ex("inoremap <C-l> HELLO"); keys("i"); QTest::keyClick(editor.get(), Qt::Key_L, Qt::ControlModifier); escape();
    QVERIFY(editor->text().contains("HELLO"));
    ex("nnoremap <F6> iFKEY<Esc>"); QTest::keyClick(editor.get(), Qt::Key_F6); QVERIFY(editor->text().contains("FKEY"));
    ex("nnoremap <Leader>x iLEADER<Esc>"); keys("\\x"); QVERIFY(editor->text().contains("LEADER"));
    ex("nnoremap Q iMAPPED<Esc>");
    ex("normal! Q"); const QString before = editor->text(); ex("normal Q"); QVERIFY(editor->text() != before);
    ex("nnoremap <buffer> Q iLOCAL<Esc>"); keys("Q"); QVERIFY(editor->text().contains("LOCAL"));
    ex("nnoremap Z <Nop>"); const QString unchanged = editor->text(); keys("Z"); QCOMPARE(editor->text(), unchanged);
}
void VimInputBindingTest::mappingModesAndRepeat()
{
    ex("onoremap Q iw"); keys("dQ"); QCOMPARE(editor->document()->line(0).text(), QString(" two three"));
    ex("xnoremap Q d"); keys("lvwQ"); QCOMPARE(editor->document()->line(0).text(), QString(" hree"));
    ex("cnoremap ZZ set"); ex("ZZ ts=7"); QCOMPARE(QDocument::tabStop(), 7);
    ex("cmapclear"); ex("nnoremap Q x"); editor->setText("abcdef", false); editor->setCursorPosition(0, 0);
    keys("2Q"); QCOMPARE(editor->text(), QString("cdef"));
    keys("."); QCOMPARE(editor->text(), QString("ef"));
    ex("iabbrev xx EXPANDED"); editor->setText("", false); editor->setCursorPosition(0, 0);
    keys("qai xx "); escape(); keys("q@a"); QCOMPARE(editor->text().count("EXPANDED"), 2);
    editor->setText("", false); editor->setCursorPosition(0, 0); keys("3ixx "); escape();
    QCOMPARE(editor->text(), QString("EXPANDED EXPANDED EXPANDED "));
    ex("nnoremap Q x"); ex("nnoremap QQ dd"); editor->setText("abcdef", false); editor->setCursorPosition(0, 0);
    keys("Qx"); QCOMPARE(editor->text(), QString("cdef"));
}

void VimInputBindingTest::mappingTimeoutAndRecursion()
{
    ex("inoremap jj <Esc>"); keys("ij");
    QCOMPARE(editor->document()->line(0).text(), QString("one two three"));
    QTest::qWait(1150); QCOMPARE(editor->document()->line(0).text(), QString("jone two three"));
    keys("jx"); QCOMPARE(editor->document()->line(0).text(), QString("jjxone two three")); escape();
    ex("nmap Q Q"); keys("Q"); QVERIFY(editor->property("vimMessage").toString().contains("recursion"));
    ex("nunmap Q"); ex("nnoremap Q x"); ex("nnoremap QQ dd"); keys("Q"); QTest::qWait(1150);
    QCOMPARE(editor->document()->line(0).text(), QString("jjone two three"));
    editor->setText("one", false); editor->setCursorPosition(0, 0); keys("ij");
    QFocusEvent focusOut(QEvent::FocusOut); QApplication::sendEvent(editor.get(), &focusOut);
    QCOMPARE(editor->text(), QString("jone")); escape();
    QTemporaryDir directory; QVERIFY(directory.isValid());
    editor->setText("one", false); editor->setCursorPosition(0, 0); QVERIFY(editor->save(directory.filePath("pending.tex")));
    keys("ij"); vim->flushPendingInput(); QVERIFY(editor->isContentModified()); QVERIFY(editor->save());
    QFile saved(editor->fileName()); QVERIFY(saved.open(QIODevice::ReadOnly)); QCOMPARE(saved.readAll(), QByteArray("jone")); escape();
    editor->setText("one", false); editor->setCursorPosition(0, 0); keys("ij"); vim->setEnabled(false);
    QCOMPARE(editor->text(), QString("jone")); vim->setEnabled(true);

}
void VimInputBindingTest::definitionsPersistence()
{
    QTemporaryDir directory; QVERIFY(directory.isValid());
    const QVariant previous = qApp->property("vimDefinitionsFile");
    const QString filename = directory.filePath("vim-definitions.json");
    qApp->setProperty("vimDefinitionsFile", filename);
    {
        QEditor other; other.document()->setFormatScheme(new QFormatScheme(other.document()));
        VimInputBinding binding(&other); other.setInputBinding(&binding); binding.setEnabled(true);
        QTest::keyClicks(&other, ":iabbrev abc SAVED"); QTest::keyClick(&other, Qt::Key_Return);
        QTest::keyClicks(&other, ":nnoremap Q iMAP<Esc>"); QTest::keyClick(&other, Qt::Key_Return);
        QVERIFY(QFile::exists(filename));
    }
    // Force a reload from disk by switching to an empty storage first.
    qApp->setProperty("vimDefinitionsFile", QString()); { QEditor empty; VimInputBinding binding(&empty); }
    qApp->setProperty("vimDefinitionsFile", filename);
    {
        QEditor other; other.document()->setFormatScheme(new QFormatScheme(other.document())); other.setText("", false);
        VimInputBinding binding(&other); other.setInputBinding(&binding); binding.setEnabled(true);
        QTest::keyClicks(&other, "iabc "); QTest::keyClick(&other, Qt::Key_Escape); QCOMPARE(other.text(), QString("SAVED "));
        QTest::keyClicks(&other, "Q"); QVERIFY(other.text().contains("MAP"));
    }
    qApp->setProperty("vimDefinitionsFile", previous);
}
void VimInputBindingTest::insertExtensions()
{
    keys("i"); QTest::keyClick(editor.get(), Qt::Key_O, Qt::ControlModifier); keys("dw");
    QCOMPARE(editor->property("vimMode").toString(), QString("VIM INSERT")); keys("NEW"); escape();
    QCOMPARE(editor->document()->line(0).text(), QString("NEWtwo three"));
    keys("i"); QTest::keyClick(editor.get(), Qt::Key_O, Qt::ControlModifier); keys("ciwREPLACED"); escape();
    QCOMPARE(editor->property("vimMode").toString(), QString("VIM NORMAL"));
    editor->setText("NEWtwo three\nfour five", false); editor->setCursorPosition(0, 0);
    ex("set sw=2"); keys("i"); QTest::keyClick(editor.get(), Qt::Key_T, Qt::ControlModifier);
    QVERIFY(editor->text().startsWith("  ")); QTest::keyClick(editor.get(), Qt::Key_D, Qt::ControlModifier);
    QVERIFY(editor->text().startsWith("NEW")); escape();
    editor->setCursorPosition(0, 0); keys("i"); QTest::keyClick(editor.get(), Qt::Key_E, Qt::ControlModifier); escape();
    QVERIFY(editor->text().startsWith("fNEW"));
}
void VimInputBindingTest::extraMotionsAndEx()
{
    editor->setText("foo.bar baz   \nsecond\nthird", false); editor->setCursorPosition(0, 8);
    keys("ge"); QCOMPARE(editor->cursor().columnNumber(), 6);
    keys("ge"); QCOMPARE(editor->cursor().columnNumber(), 3);
    editor->setCursorPosition(0, 8); keys("gE"); QCOMPARE(editor->cursor().columnNumber(), 6);
    keys("g_"); QCOMPARE(editor->cursor().columnNumber(), 10);
    keys("5go"); QCOMPARE(editor->cursor().columnNumber(), 4);
    keys("gj"); QCOMPARE(editor->cursor().lineNumber(), 1);
    keys("gk"); QCOMPARE(editor->cursor().lineNumber(), 0);
    editor->setText("b\na\nc", false); editor->setCursorPosition(0, 0); ex("sort"); QCOMPARE(editor->text(), QString("b\na\nc"));
    ex("%sort"); QCOMPARE(editor->text(), QString("a\nb\nc"));
    ex("s/a/X/"); keys("j"); ex("&"); QCOMPARE(editor->text(), QString("X\nb\nc"));
    QTemporaryDir directory; QFile file(directory.filePath("commands.vim")); QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("\" supported commands only\niabbrev src SOURCE\nnnoremap Q iYES<Esc>\n"); file.close();
    ex("source " + file.fileName()); keys("Q"); QVERIFY(editor->text().contains("YES"));
    ex("mapclear");
    editor->setText("one\ntwo\nthree\nfour", false); editor->setCursorPosition(0, 0);
    ex("1,2copy $"); QCOMPARE(editor->text(), QString("one\ntwo\nthree\nfour\none\ntwo"));
    keys("u"); QCOMPARE(editor->text(), QString("one\ntwo\nthree\nfour"));
    ex("1,2move $"); QCOMPARE(editor->text(), QString("three\nfour\none\ntwo"));
    keys("u"); keys("yy"); ex("put"); QCOMPARE(editor->text(), QString("one\none\ntwo\nthree\nfour"));
    keys("u"); editor->setCursorPosition(0, 0); keys("ma"); ex("'a+1,$-1d"); QCOMPARE(editor->text(), QString("one\nfour"));
    keys("u"); editor->setCursorPosition(0, 0); ex("/two/;+1d"); QCOMPARE(editor->text(), QString("one\nfour"));
    keys("u"); editor->setCursorPosition(0, 0); ex("2;+1d"); QCOMPARE(editor->text(), QString("one\nfour"));
    editor->setText("original", false); editor->setCursorPosition(0, 0); keys("3oX"); escape();
    QCOMPARE(editor->text(), QString("original\nX\nX\nX")); keys("u"); QCOMPARE(editor->text(), QString("original"));
    keys("3OX"); escape(); QCOMPARE(editor->text(), QString("X\nX\nX\noriginal"));
    editor->setText("abc", false); editor->setCursorPosition(0, 0); keys("iHELLO"); escape(); keys("$gi!"); escape();
    QCOMPARE(editor->text(), QString("HELLO!abc"));

}


void VimInputBindingTest::extensionOracle_data()
{
    QTest::addColumn<QString>("text"); QTest::addColumn<int>("position"); QTest::addColumn<QStringList>("definitions");
    QTest::addColumn<QString>("input"); QTest::addColumn<QString>("expected"); QTest::addColumn<int>("row"); QTest::addColumn<int>("column");
    QFile file(QFINDTESTDATA("vim_extension_oracle.json")); QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonDocument reference = QJsonDocument::fromJson(file.readAll());
    QFile nvim(QFINDTESTDATA("nvim_extension_oracle.json")); QVERIFY(nvim.open(QIODevice::ReadOnly));
    QCOMPARE(reference, QJsonDocument::fromJson(nvim.readAll()));
    for (const QJsonValue &value : reference.array()) {
        const QJsonObject item = value.toObject(); QStringList definitions;
        for (const auto &definition : item["definitions"].toArray()) definitions.append(definition.toString());
        QTest::newRow(item["name"].toString().toUtf8().constData()) << item["text"].toString() << item["pos"].toInt() << definitions
            << item["keys"].toString() << item["expected"].toString() << item["line"].toInt() << item["column"].toInt();
    }
}
void VimInputBindingTest::extensionOracle()
{
    QFETCH(QString, text); QFETCH(int, position); QFETCH(QStringList, definitions); QFETCH(QString, input);
    QFETCH(QString, expected); QFETCH(int, row); QFETCH(int, column);
    for (const QString &definition : definitions) ex(definition);
    editor->setText(text, false); editor->setCursorPosition(0, position);
    for (QChar ch : input) {
        if (ch.unicode() == 27) escape();
        else if (ch.unicode() == 29) QTest::keyClick(editor.get(), Qt::Key_BracketRight, Qt::ControlModifier);
        else if (ch.unicode() == 3) QTest::keyClick(editor.get(), Qt::Key_C, Qt::ControlModifier);
        else if (ch == '\r') QTest::keyClick(editor.get(), Qt::Key_Return);
        else QTest::keyClicks(editor.get(), QString(ch));
    }
    QCOMPARE(editor->document()->textLines().join('\n'), expected);
    QCOMPARE(editor->cursor().lineNumber(), row);
    QCOMPARE(editor->cursor().columnNumber(), column);
}

#endif
