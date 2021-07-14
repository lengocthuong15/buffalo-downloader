/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef HTTPWINDOW_H
#define HTTPWINDOW_H

#include <QProgressDialog>
#include <QNetworkAccessManager>
#include <QUrl>
#include <QThread>

QT_BEGIN_NAMESPACE
class QFile;
class QLabel;
class QLineEdit;
class QPushButton;
class QSslError;
class QAuthenticator;
class QNetworkReply;
class QCheckBox;

QT_END_NAMESPACE

class ProgressDialog : public QProgressDialog {
    Q_OBJECT

public:
    explicit ProgressDialog(const QUrl &url, QWidget *parent = nullptr);

public slots:
    void networkReplyProgress(qint64 bytesRead, qint64 totalBytes);
};

struct REQUEST_PARAM {
    QUrl url;
    QString user;
    QString password;
    quint64 start;
    quint64 end;
    QString proxyName;
    int proxyPort;
};

struct TEMP_FILE {
    QFile *file = nullptr;
    bool isFinished = false;
    TEMP_FILE() {};
    TEMP_FILE(QFile * file): file(file){
    };
};

class DownloadWorker : public QThread
{
    Q_OBJECT

public:
    DownloadWorker(const REQUEST_PARAM &rqParam, TEMP_FILE &tempFile);
    DownloadWorker();
    void setFile(QFile *file) {
        this->tempFile.file = file;
    }
    bool isIdle() {
        return !this->tempFile.file;
    }
    void run() override;
public slots:
    void readyRead();
    void doneRead();
signals:
    void download_done(const QString &fileName);
    void reply_progress(qint64 bytesRead);

protected:
    QNetworkAccessManager qnam;
    REQUEST_PARAM rqParam;
    TEMP_FILE tempFile;
    QNetworkReply *reply;
};


class HttpWindow : public QDialog
{
    Q_OBJECT

public:
    explicit HttpWindow(QWidget *parent = nullptr);

    void startRequest(const QUrl &requestedUrl);

private:
    quint64 getContentLength(const QUrl &requestedUrl);
    DownloadWorker* pickWorker();

signals:
    void download_progress_signal(qint64 bytesRead, qint64 totalBytes);
    void download_done_signal();

private slots:
    void downloadFile();
    void cancelDownload();
    void httpFinished();
    void rebuildFile(const QString &fileName);
    void httpReadyRead();
    void enableDownloadButton();
    void slotAuthenticationRequired(QNetworkReply *, QAuthenticator *authenticator);
    void download_progress(qint64 bytesRead);
#ifndef QT_NO_SSL
    void sslErrors(QNetworkReply *, const QList<QSslError> &errors);
#endif

private:
    QFile *openFileForWrite(const QString &fileName);
    QFile *openFileForRead(const QString &fileName);

    QLabel *statusLabel;
    QLineEdit *urlLineEdit;
    QPushButton *downloadButton;
    QCheckBox *launchCheckBox;
    QLineEdit *defaultFileLineEdit;
    QLineEdit *downloadDirectoryLineEdit;
    QLineEdit *userNameEdit;
    QLineEdit *passEdit;
    QLineEdit *proxyServerEdit;
    QLineEdit *proxyPortEdit;

    QUrl url;
    QNetworkAccessManager *qnam;
    QNetworkReply *reply;
    QFile *file;
    QVector<TEMP_FILE> filePools;
    QVector<DownloadWorker*> downloadWorkerPools;
    bool httpRequestAborted;
    quint64 totalBytes;
    quint64 currentBytes = 0;
};

#endif
