// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 rodioniurev
// Local mock endpoints only; live Ollama is an explicit optional test.
#include "texstudio.h"
#include "aieditdialog.h"
#include "latexeditorview.h"
#include "configdialog.h"
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
#include <QCheckBox>
#include <QToolBar>
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
    if (argc>4 && QString::fromLocal8Bit(argv[3])=="--verify-key") {
        AIEditDialog settingsPage(nullptr,QString::fromLocal8Bit(argv[4]),0,&window,true);
        CHECK(settingsPage.findChild<QLineEdit *>("aiEditKey")->text()=="settings-test-key");
        QTextStream(stdout)<<"PASS: stored key restored in a fresh process.\n"; return 0;
    }
    auto *menu=window.findChild<QMenu *>("main/wizards/aiedit"); CHECK(menu);
    CHECK(window.findChild<QAction *>("main/wizards/aiedit/custom"));
    auto *quickAction=window.findChild<QAction *>("main/wizards/aiedit/custom");
    if (qEnvironmentVariableIsSet("TXS_TEST_EXPECT_REGIONAL")) CHECK(quickAction->text()==QString::fromUtf8("Изменить с ИИ..."));
    CHECK(LatexEditorView::getBaseActions().contains(quickAction));
    CHECK(window.findChild<QToolBar *>("File")->actions().contains(quickAction));
    {
        ConfigDialog configuration(&window);
        CHECK(configuration.findChild<AIEditDialog *>("aiEditSettingsPage"));
        configuration.ui.contentsWidget->setCurrentRow(configuration.ui.pagesWidget->count()-1);
        configuration.resize(1050,740); configuration.show(); app.processEvents();
        configuration.grab().save(QString::fromLocal8Bit(argv[2])+"/ai-configuration.png");
        configuration.reject();
    }
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
        dialog.findChild<QCheckBox *>("aiEditRelated")->setChecked(false);
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
        dialog.show(); app.processEvents();
        CHECK(!dialog.findChild<QComboBox *>("aiEditProvider")->isVisible());
        CHECK(dialog.findChild<QPlainTextEdit *>("aiEditPrompt")->isVisible());
        CHECK(!dialog.findChild<QPlainTextEdit *>("aiEditOriginal")->isVisible());
        dialog.grab().save(QString::fromLocal8Bit(argv[2])+"/ai-quick-prompt.png");
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
        selectFrame(); AIEditDialog dialog(&editor,settings); configure(dialog,1);
        dialog.findChild<QCheckBox *>("aiEditRelated")->setChecked(true);
        const QString setup="\\documentclass{beamer}\n\\newcommand{\\lang}{es}";
        const QJsonObject proposal{{"edits",QJsonArray{QJsonObject{{"old","\\documentclass{beamer}"},{"new",setup}},QJsonObject{{"old",selection},{"new",changed}}}},{"summary","Language variable and related slide change"}};
        response=QJsonDocument(QJsonObject{{"choices",QJsonArray{QJsonObject{{"message",QJsonObject{{"content",QString::fromUtf8(QJsonDocument(proposal).toJson())}}}}}}}).toJson();
        dialog.generate(); CHECK(finish(dialog)); CHECK(editor.document()->text()==original);
        const QJsonObject sent=QJsonDocument::fromJson(lastRequest).object();
        const auto input=QJsonDocument::fromJson(sent.value("messages").toArray().last().toObject().value("content").toString().toUtf8()).object();
        CHECK(input.value("document")==original && input.value("selection")==selection);
        CHECK(dialog.findChild<QPushButton *>("aiEditApply")->isEnabled());
        CHECK(dialog.findChild<QPlainTextEdit *>("aiEditProposed")->toPlainText().contains("\\newcommand{\\lang}{es}"));
        dialog.applyChanges(); CHECK(editor.document()->text().contains(setup)); CHECK(editor.document()->text().contains(changed)); editor.undo(); CHECK(editor.document()->text()==original);
    }
    {
        selectFrame(); AIEditDialog dialog(&editor,settings); configure(dialog,1);
        response=QJsonDocument(QJsonObject{{"choices",QJsonArray{QJsonObject{{"message",QJsonObject{{"content",QString::fromUtf8(QJsonDocument(QJsonObject{{"edits",QJsonArray{QJsonObject{{"old","\\documentclass{beamer}"},{"new","outside selection"}}}}}).toJson())}}}}}}}).toJson();
        dialog.generate(); CHECK(finish(dialog)); CHECK(!dialog.findChild<QPushButton *>("aiEditApply")->isEnabled()); CHECK(editor.document()->text()==original);
    }
    {
        const QString connectionSettings=scratch.filePath("connection.ini");
        {
            AIEditDialog settingsPage(nullptr,connectionSettings,0,&window,true);
            configure(settingsPage,1); settingsPage.findChild<QLineEdit *>("aiEditKey")->setText("settings-test-key");
            settingsPage.findChild<QCheckBox *>("aiEditRememberKey")->setChecked(true);
            CHECK(settingsPage.commitSettings());
        }
        QFile saved(connectionSettings); CHECK(saved.open(QIODevice::ReadOnly)); const QByteArray bytes=saved.readAll(); saved.close(); CHECK(!bytes.contains("settings-test-key"));
        {
            AIEditDialog cancelled(nullptr,connectionSettings,0,&window,true);
            cancelled.findChild<QLineEdit *>("aiEditKey")->setText("discard-this-key"); cancelled.reject();
        }
        CHECK(saved.open(QIODevice::ReadOnly)); CHECK(saved.readAll()==bytes); saved.close();
#ifdef Q_OS_WIN
        QProcess verify; verify.start(app.applicationFilePath(),{QString::fromLocal8Bit(argv[1]),QString::fromLocal8Bit(argv[2]),"--verify-key",connectionSettings});
        CHECK(verify.waitForFinished(30000)); CHECK(verify.exitCode()==0);
#endif
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
    for(int variant=0;variant<5;++variant) {
        AIEditDialog dialog(&editor,settings); configure(dialog,0);
        const QString json=QString::fromUtf8(QJsonDocument(QJsonObject{{"replacement",changed},{"summary","Streamed edit"}}).toJson(QJsonDocument::Compact));
        auto frame=[](const QString &text,bool done) {return QJsonDocument(QJsonObject{{"message",QJsonObject{{"content",text}}},{"done",done},{"done_reason","stop"}}).toJson(QJsonDocument::Compact);};
        response=frame(json.left(json.size()/2),false)+"\n"+frame(json.mid(json.size()/2),variant!=2);
        if(variant==1)response+="\n";
        if(variant==3)response+="\n{\"error\":\"stream error\"}";
        if(variant==4)response+="\ninvalid tail";
        dialog.generate(); CHECK(finish(dialog));
        CHECK(QJsonDocument::fromJson(lastRequest).object().value("stream").toBool());
        CHECK(dialog.findChild<QPushButton *>("aiEditApply")->isEnabled()==(variant<2));
        CHECK(editor.document()->text()==original);
    }
    QTextStream(stdout)<<"PASS: quick dialog, direct context action, toolbar button, Configuration page, related edits outside the selection, atomic undo, scope restriction, encrypted key persistence/fresh-process restore, Configuration cancel, provider payloads, malformed/truncated replies and stale-document guard.\n";
    if (qEnvironmentVariableIsSet("TXS_TEST_LIVE_OLLAMA")) {
        const QString liveModel=qEnvironmentVariable("TXS_TEST_OLLAMA_MODEL","qwen3:4b-instruct");
        auto compile=[&]() {
            const QString latex=qEnvironmentVariable("TXS_TEST_LATEX"); if (latex.isEmpty()) return true;
            for (const QString &extension : {"aux","toc","out","nav","snm"}) QFile::remove(scratch.filePath("generated."+extension));
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
        const QString calendar="\\documentclass{article}\n\\usepackage[utf8]{inputenc}\n\\usepackage[T2A]{fontenc}\n\\usepackage[russian]{babel}\n\\newcommand{\\calMonth}{11}\n\\newcommand{\\calMonthName}{Noviembre}\n\\newcommand{\\weekdayNames}{Lunes, Martes, Miercoles, Jueves, Viernes, Sabado, Domingo}\n\\begin{document}\n\\calMonthName\n\n\\weekdayNames\n\\end{document}";
        editor.setText(calendar,false); editor.setCursor(QDocumentCursor(editor.document(),5,0,5,37));
        AIEditDialog language(&editor,settings); configure(language,0); language.findChild<QCheckBox *>("aiEditRelated")->setChecked(true);
        language.findChild<QLineEdit *>("aiEditEndpoint")->setText("http://127.0.0.1:11434/api/chat"); language.findChild<QLineEdit *>("aiEditKey")->clear();
        language.findChild<QComboBox *>("aiEditModel")->setEditText(liveModel); language.findChild<QSpinBox *>("aiEditContext")->setValue(8192); language.findChild<QSpinBox *>("aiEditTokens")->setValue(4096);
        language.findChild<QPlainTextEdit *>("aiEditPrompt")->setPlainText("Add \\newcommand{\\lang}{es} near calMonth. Replace the hardcoded calMonthName with a function depending on calMonth (all 12 months) and lang (es or en). Also update weekdayNames elsewhere in the document: all seven weekday names must depend on lang. Keep the original calMonth variable and document structure. Default es, Spanish. en selects English. Copy every old fragment exactly from the source. Use plain TeX or etoolbox if necessary; add any dependency in the preamble.");
        language.generate(); CHECK(waitFor([&] {return language.findChild<QPushButton *>("aiEditGenerate")->isEnabled();},610000));
        if (!language.findChild<QPushButton *>("aiEditApply")->isEnabled()) QTextStream(stderr)<<language.findChild<QLabel *>("aiEditStatus")->text()<<'\n'<<language.findChild<QPlainTextEdit *>("aiEditRaw")->toPlainText()<<'\n';
        CHECK(language.findChild<QPushButton *>("aiEditApply")->isEnabled()); language.applyChanges(); const QString localized=editor.document()->text(); CHECK(localized!=calendar);
        const QString extractor=qEnvironmentVariable("TXS_TEST_PDFTOTEXT");
        if (!extractor.isEmpty()) {
            for (const auto &entry : {qMakePair(QString("es"),QString("Noviembre")),qMakePair(QString("en"),QString("November"))}) for (const int month : {11,3}) {
                QString variant=localized;
                const QRegularExpression langPattern("(\\\\newcommand\\s*\\{\\\\lang\\}\\s*\\{)es(\\})");
                CHECK(langPattern.match(variant).hasMatch()); variant.replace(langPattern,"\\1"+entry.first+"\\2");
                const QRegularExpression monthPattern("(\\\\newcommand\\s*\\{\\\\calMonth\\}\\s*\\{)11(\\})");
                const auto monthMatch=monthPattern.match(variant); CHECK(monthMatch.hasMatch());
                variant.replace(monthMatch.capturedStart(),monthMatch.capturedLength(),monthMatch.captured(1)+QString::number(month)+monthMatch.captured(2));
                editor.setText(variant,false); CHECK(compile());
                QProcess extract; extract.setWorkingDirectory(scratch.path()); extract.start(extractor,{"generated.pdf","-"}); CHECK(extract.waitForFinished(30000)); CHECK(extract.exitCode()==0);
                const QString pdf=QString::fromUtf8(extract.readAllStandardOutput()).toLower();
                CHECK(pdf.contains(month==11 ? entry.second.toLower() : entry.first=="es" ? "marzo" : "march")); CHECK(pdf.contains(entry.first=="es" ? "lunes" : "monday"));
            }
        }
        QTextStream(stdout)<<"PASS: live selected month-name request changed related weekday names; compiled PDF switches Spanish/English names from lang.\n";
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
