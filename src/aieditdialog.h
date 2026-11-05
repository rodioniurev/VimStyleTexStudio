// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 rodioniurev
#ifndef AIEDITDIALOG_H
#define AIEDITDIALOG_H
#include <QDialog>
#include <QPointer>
#include <QJsonObject>
#include <QHash>
#include <memory>
#include "qdocumentcursor.h"
class QEditor;
class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QLabel;
class QCheckBox;
class QSpinBox;
class QNetworkAccessManager;
class QNetworkReply;
class QProcess;
class QTimer;
class QTemporaryDir;

class AIEditDialog : public QDialog {
    Q_OBJECT
public:
    explicit AIEditDialog(QEditor *editor, const QString &settingsFile, int preset = 0, QWidget *parent = nullptr, bool settingsMode = false);
    ~AIEditDialog() override;
    bool commitSettings();
signals:
    void settingsRequested();
public slots:
    void generate();
    void checkConnection();
    void applyChanges();
    void reject() override;
private:
    enum Provider { Ollama, OpenAI, Claude, Compatible, Copilot };
    QPointer<QEditor> m_editor;
    QDocumentCursor m_selection;
    QString m_snapshot, m_settingsFile, m_source, m_keyForRequest, m_finalSettingsFile;
    std::unique_ptr<QTemporaryDir> m_temporarySettings;
    QHash<int,QString> m_settingKeys;
    bool m_settingsMode = false, m_applyWhole = false;
    QWidget *m_review, *m_settingsPanel;
    int m_previousProvider = -1;
    QComboBox *m_provider, *m_model, *m_scope, *m_preset;
    QLineEdit *m_endpoint, *m_key;
    QPlainTextEdit *m_prompt, *m_original, *m_proposed, *m_raw;
    QPushButton *m_generate, *m_check, *m_apply, *m_stop;
    QLabel *m_status, *m_destination;
    QCheckBox *m_preamble, *m_related, *m_rememberKey;
    QSpinBox *m_tokens, *m_context;
    QNetworkAccessManager *m_network;
    QPointer<QNetworkReply> m_reply;
    QProcess *m_process;
    QTimer *m_timeout;
    bool m_busy = false, m_proposalReady = false, m_checking = false, m_cancelled = false;
    int provider() const;
    void providerChanged();
    void scopeChanged();
    void presetChanged();
    void saveProvider();
    void setBusy(bool busy);
    void stopRequest();
    void showError(QString error);
    void handleResponse(const QByteArray &data);
    void handleModels(const QJsonObject &object);
    void setProposal(const QString &text);
    void startCopilot(const QJsonObject &input);
    QString systemPrompt() const;
    QString userPrompt() const;
    bool validateConnection();
    void request(bool models);
};
#endif
