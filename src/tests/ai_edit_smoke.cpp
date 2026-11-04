// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 rodioniurev
// Local mock endpoints only; live Ollama is an explicit optional test.
#include "texstudio.h"
#include "aieditdialog.h"
#include "latexeditorview.h"
#include "qeditor.h"
#include "qdocument.h"
#include "qformatscheme.h"
#include <QtTest/QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QComboBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QLabel>
#include <QSettings>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMenu>
#include <QTranslator>
#include <memory>
#define CHECK(v) do { if (!(v)) { QTextStream(stderr)<<"FAIL "<<__LINE__<<": "<<#v<<'\n'; return 1; } } while(false)
static bool waitFor(const std::function<bool()> &predicate, int timeout=10000) {
    QElapsedTimer timer; timer.start();
    while (!predicate() && timer.elapsed()<timeout) { QApplication::processEvents(); QTest::qWait(5); }
    return predicate();
}
int main(int argc,char **argv) {
    QApplication app(argc,argv); CHECK(argc>=3);
    ConfigManager::configDirOverride=QString::fromLocal8Bit(argv[1]); ConfigManager::dontRestoreSession=true;
    Texstudio window;
    auto *menu=window.findChild<QMenu *>("main/wizards/aiedit"); CHECK(menu);
    CHECK(window.findChild<QAction *>("main/wizards/aiedit/custom"));
    CHECK(LatexEditorView::getBaseActions().contains(menu->menuAction()));
    QTemporaryDir scratch; CHECK(scratch.isValid()); const QString settings=scratch.filePath("ai-edit.ini");
    const QString original="\\documentclass{beamer}\n\\begin{document}\n\\begin{frame}{Test}\nA then B then C\n\\end{frame}\n\\end{document}";
    const QString selection="\\begin{frame}{Test}\nA then B then C\n\\end{frame}";
    const QString changed="\\begin{frame}{Test}\n\\begin{itemize}\n\\item<1-> A\n\\item<2-> B\n\\item<3-> C\n\\end{itemize}\n\\end{frame}";
    QEditor editor; editor.document()->setFormatScheme(new QFormatScheme(editor.document())); editor.setText(original,false);
    auto selectFrame=[&] { QDocumentCursor cursor(editor.document(),2,0,4,11); editor.setCursor(cursor); };
    selectFrame(); CHECK(editor.cursor().selectedText()==selection);
    QTcpServer server; CHECK(server.listen(QHostAddress::LocalHost,0));
    QByteArray response, lastRequest, lastHeader; int status=200; bool hold=false; int requests=0;
    QObject::connect(&server,&QTcpServer::newConnection,[&] {
        auto *socket=server.nextPendingConnection(); auto buffer=std::make_shared<QByteArray>(); auto handled=std::make_shared<bool>(false);
        QObject::connect(socket,&QTcpSocket::readyRead,[&,socket,buffer,handled] {
            buffer->append(socket->readAll()); if (*handled) return;
            const int end=buffer->indexOf("\r\n\r\n"); if (end<0) return;
            const QByteArray header=buffer->left(end); const auto match=QRegularExpression("Content-Length: (\\d+)",QRegularExpression::CaseInsensitiveOption).match(QString::fromLatin1(header));
            const int length=match.hasMatch()?match.captured(1).toInt():0;
            if (buffer->size()<end+4+length) return;
            *handled=true; ++requests; lastHeader=header; lastRequest=buffer->mid(end+4,length);
            if (!hold) { socket->write("HTTP/1.1 "+QByteArray::number(status)+" Result\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: "+QByteArray::number(response.size())+"\r\n\r\n"+response); socket->disconnectFromHost(); }
        });
        QObject::connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
    });
    const QString endpoint=QString("http://127.0.0.1:%1/v1/chat/completions").arg(server.serverPort());
    auto configure=[&](AIEditDialog &dialog,int provider) {
        dialog.findChild<QComboBox *>("aiEditProvider")->setCurrentIndex(provider);
        dialog.findChild<QLineEdit *>("aiEditEndpoint")->setText(endpoint);
        dialog.findChild<QLineEdit *>("aiEditKey")->setText("test-secret-key");
        dialog.findChild<QComboBox *>("aiEditModel")->setEditText("mock-model");
        dialog.findChild<QPlainTextEdit *>("aiEditPrompt")->setPlainText("Arrange A then B then C.");
    };
    auto finish=[&](AIEditDialog &dialog) { return waitFor([&] { return dialog.findChild<QPushButton *>("aiEditGenerate")->isEnabled(); }); };
    auto output=[&](const QString &text,const QString &reason="stop") {
        const QString json=QString::fromUtf8(QJsonDocument(QJsonObject{{"replacement",text},{"summary","Updated layout"}}).toJson(QJsonDocument::Compact));
        return QJsonDocument(QJsonObject{{"choices",QJsonArray{QJsonObject{{"finish_reason",reason},{"message",QJsonObject{{"content",json}}}}}}}).toJson(QJsonDocument::Compact);
    };
    {
        AIEditDialog dialog(&editor,settings,2); configure(dialog,1); response=output(changed);
        CHECK(dialog.findChild<QLineEdit *>("aiEditKey")->echoMode()==QLineEdit::Password);
        dialog.generate(); CHECK(finish(dialog));
        CHECK(editor.document()->text()==original);
        const auto sent=QJsonDocument::fromJson(lastRequest).object();
        CHECK(sent.contains("max_completion_tokens") && !sent.contains("tools"));
        const auto user=QJsonDocument::fromJson(sent.value("messages").toArray().last().toObject().value("content").toString().toUtf8()).object();
        CHECK(user.value("source")==selection && user.value("read_only_preamble").toString().contains("documentclass"));
        CHECK(lastHeader.contains("Bearer test-secret-key"));
        CHECK(dialog.findChild<QPushButton *>("aiEditApply")->isEnabled());
        dialog.resize(1100,850); dialog.show(); app.processEvents(); dialog.grab().save(QString::fromLocal8Bit(argv[2])+"/ai-edit-review.png");
        dialog.applyChanges(); CHECK(editor.document()->text()==QString(original).replace(selection,changed));
        editor.undo(); CHECK(editor.document()->text()==original);
    }
    {
        selectFrame(); AIEditDialog dialog(&editor,settings); configure(dialog,2);
        const QString json=QString::fromUtf8(QJsonDocument(QJsonObject{{"replacement",changed},{"summary","Claude test"}}).toJson(QJsonDocument::Compact));
        response=QJsonDocument(QJsonObject{{"stop_reason","end_turn"},{"content",QJsonArray{QJsonObject{{"type","thinking"},{"thinking","not source"}},QJsonObject{{"type","text"},{"text",json}}}}}).toJson();
        dialog.generate(); CHECK(finish(dialog)); const auto sent=QJsonDocument::fromJson(lastRequest).object();
        CHECK(sent.contains("max_tokens") && sent.contains("system") && sent.value("messages").toArray().size()==1);
        CHECK(lastHeader.toLower().contains("anthropic-version") && lastHeader.contains("test-secret-key"));
        CHECK(dialog.findChild<QPlainTextEdit *>("aiEditProposed")->toPlainText()==changed);
        editor.setText(original+"\nexternal edit",false); dialog.applyChanges(); CHECK(editor.document()->text().endsWith("external edit"));
    }
    editor.setText(original,false); selectFrame();
    for (int variant=0;variant<4;++variant) {
        AIEditDialog dialog(&editor,settings); configure(dialog,3); status=200;
        if (variant==0) response=output(changed,"length");
        if (variant==1) response="{\"choices\":[{\"message\":{\"content\":\"not JSON\"}}]}";
        if (variant==2) { status=401; response="{\"error\":{\"message\":\"bad key test-secret-key\"}}"; }
        if (variant==3) response=output("");
        dialog.generate(); CHECK(finish(dialog)); CHECK(!dialog.findChild<QPushButton *>("aiEditApply")->isEnabled());
        CHECK(editor.document()->text()==original);
        if (variant==2) CHECK(!dialog.findChild<QLabel *>("aiEditStatus")->text().contains("test-secret-key"));
    }
    status=200;
    {
        AIEditDialog dialog(&editor,settings); configure(dialog,3); hold=true; int count=requests;
        dialog.generate(); CHECK(waitFor([&] {return requests>count;})); dialog.reject(); hold=false;
        CHECK(editor.document()->text()==original);
    }
    {
        AIEditDialog dialog(&editor,settings); configure(dialog,0); response="{\"models\":[{\"name\":\"local-test\"}]}";
        dialog.findChild<QLineEdit *>("aiEditEndpoint")->setText(QString("http://127.0.0.1:%1/api/chat").arg(server.serverPort()));
        dialog.checkConnection(); CHECK(finish(dialog)); CHECK(dialog.findChild<QComboBox *>("aiEditModel")->findText("local-test")>=0);
        CHECK(lastHeader.startsWith("GET /api/tags "));
    }
    QFile persisted(settings); CHECK(persisted.open(QIODevice::ReadOnly)); CHECK(!persisted.readAll().contains("test-secret-key"));
    QTextStream(stdout)<<"PASS: editor menu, selection/preamble scope, OpenAI/Claude payloads, multi-block text, proposal review, exact replacement, single undo, stale document guard, truncation, invalid/empty replies, HTTP errors/key redaction, cancellation, model discovery and no key persistence.\n";
    if (qEnvironmentVariableIsSet("TXS_TEST_LIVE_OLLAMA")) {
        const QString liveModel=qEnvironmentVariable("TXS_TEST_OLLAMA_MODEL","qwen3:4b-instruct");
        auto compile=[&]() {
            const QString latex=qEnvironmentVariable("TXS_TEST_LATEX"); if (latex.isEmpty()) return true;
            QFile captured(QString::fromLocal8Bit(argv[2])+"/ai-edit-example-result.tex");
            if (captured.open(QIODevice::WriteOnly)) captured.write(editor.document()->text().toUtf8());
            QFile file(scratch.filePath("generated.tex")); if (!file.open(QIODevice::WriteOnly)) return false;
            file.write(editor.document()->text().toUtf8()); file.close();
            QProcess process; process.setWorkingDirectory(scratch.path());
            process.start(latex,{"-interaction=nonstopmode","-halt-on-error","-no-shell-escape","generated.tex"});
            if (!process.waitForFinished(90000) || process.exitCode()!=0) { QTextStream(stderr)<<editor.document()->text()<<'\n'<<process.readAllStandardOutput().right(3000); return false; }
            return QFileInfo::exists(scratch.filePath("generated.pdf"));
        };
        editor.setText("\\documentclass{article}\n\\begin{document}\nHello world.\n\\end{document}",false); editor.setCursorPosition(0,0);
        AIEditDialog dialog(&editor,settings,1); configure(dialog,0);
        dialog.findChild<QLineEdit *>("aiEditEndpoint")->setText("http://127.0.0.1:11434/api/chat");
        dialog.findChild<QLineEdit *>("aiEditKey")->clear(); dialog.findChild<QComboBox *>("aiEditModel")->setEditText(liveModel);
        dialog.findChild<QSpinBox *>("aiEditContext")->setValue(8192); dialog.findChild<QSpinBox *>("aiEditTokens")->setValue(1024);
        dialog.findChild<QPlainTextEdit *>("aiEditPrompt")->setPlainText("Add xcolor with text color dark blue, preserve Hello world, return the whole modified document.");
        dialog.generate(); CHECK(waitFor([&] {return dialog.findChild<QPushButton *>("aiEditGenerate")->isEnabled();},295000));
        if (!dialog.findChild<QPushButton *>("aiEditApply")->isEnabled()) QTextStream(stderr)<<dialog.findChild<QLabel *>("aiEditStatus")->text()<<'\n'<<dialog.findChild<QPlainTextEdit *>("aiEditRaw")->toPlainText()<<'\n';
        CHECK(dialog.findChild<QPushButton *>("aiEditApply")->isEnabled());
        CHECK(dialog.findChild<QPlainTextEdit *>("aiEditProposed")->toPlainText().contains("Hello world"));
        const QString before=editor.document()->text(); dialog.applyChanges(); CHECK(editor.document()->text()!=before); CHECK(compile()); editor.undo(); CHECK(editor.document()->text()==before);
        editor.setText(original,false); selectFrame();
        AIEditDialog beamer(&editor,settings,2); configure(beamer,0);
        beamer.findChild<QLineEdit *>("aiEditEndpoint")->setText("http://127.0.0.1:11434/api/chat"); beamer.findChild<QLineEdit *>("aiEditKey")->clear();
        beamer.findChild<QComboBox *>("aiEditModel")->setEditText(liveModel);
        beamer.findChild<QSpinBox *>("aiEditContext")->setValue(8192); beamer.findChild<QSpinBox *>("aiEditTokens")->setValue(1536);
        beamer.findChild<QPlainTextEdit *>("aiEditPrompt")->setPlainText("Use itemize with overlays to reveal A, then B, then C on this frame. Preserve its title and all three letters. Return only the replacement frame, not the preamble or document wrapper.");
        beamer.generate(); CHECK(waitFor([&] {return beamer.findChild<QPushButton *>("aiEditGenerate")->isEnabled();},295000));
        if (!beamer.findChild<QPushButton *>("aiEditApply")->isEnabled()) QTextStream(stderr)<<beamer.findChild<QLabel *>("aiEditStatus")->text()<<'\n'<<beamer.findChild<QPlainTextEdit *>("aiEditRaw")->toPlainText()<<'\n';
        CHECK(beamer.findChild<QPushButton *>("aiEditApply")->isEnabled()); beamer.applyChanges();
        CHECK(editor.document()->text().contains("\\begin{frame}")); CHECK(compile()); editor.undo(); CHECK(editor.document()->text()==original);
        QTextStream(stdout)<<"PASS: live local "<<liveModel<<" generated color and Beamer edits; explicit apply, LaTeX compilation and undo succeeded.\n";
    }
    if (qEnvironmentVariableIsSet("TXS_TEST_RUSSIAN_QM")) {
        QTranslator translation; CHECK(translation.load(qEnvironmentVariable("TXS_TEST_RUSSIAN_QM"))); app.installTranslator(&translation);
        AIEditDialog dialog(&editor,settings,1);
        CHECK(dialog.findChild<QPushButton *>("aiEditApply")->text()==QString::fromUtf8("Применить изменения"));
        dialog.findChild<QLineEdit *>("aiEditKey")->clear(); dialog.show(); app.processEvents();
        dialog.grab().save(QString::fromLocal8Bit(argv[2])+"/ai-edit-russian.png");
        QTextStream(stdout)<<"PASS: Russian AI editing labels and task prompts.\n";
    }
    return 0;
}
