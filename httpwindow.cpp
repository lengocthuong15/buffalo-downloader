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

#include <QtWidgets>
#include <QtNetwork>
#include <QUrl>
#include <QHttp2Configuration>

#include "httpwindow.h"
#include "ui_authenticationdialog.h"

#if QT_CONFIG(ssl)
const char defaultUrl[] = "https://www.qt.io/";
#else
const char defaultUrl[] = "http://www.qt.io/";
#endif
const QString CONTENT_LENGTH = "Content-Length";
const char defaultFileName[] = "index.html";

ProgressDialog::ProgressDialog(const QUrl &url, QWidget *parent)
    : QProgressDialog(parent)
{
    setWindowTitle(tr("Download Progress"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setLabelText(tr("Downloading %1.").arg(url.toDisplayString()));
    setMinimum(0);
    setValue(0);
    setMinimumDuration(0);
    setMinimumSize(QSize(400, 75));
}

void ProgressDialog::networkReplyProgress(qint64 bytesRead, qint64 totalBytes)
{
    setMaximum(totalBytes);
    setValue(bytesRead);
}

DownloadWorker::DownloadWorker(const REQUEST_PARAM &rqParam, TEMP_FILE &tempFile)
    : QThread(), rqParam(rqParam), tempFile(tempFile)
{
    QThread::moveToThread(this);
    qnam.moveToThread(this);
    if (!rqParam.proxyName.isEmpty() && rqParam.proxyPort) {
            QNetworkProxy proxy;
            proxy.setType(QNetworkProxy::HttpProxy);
            proxy.setHostName(rqParam.proxyName);
            proxy.setPort(rqParam.proxyPort);
            qnam.setProxy(proxy);
        }
}

void DownloadWorker::readyRead() {
    if (this->isCancle) {
        qDebug() << "Ready read but the requested is cancled";
        return;
    }
    qint64 readByte = reply->bytesAvailable();
    qDebug() << "ReadyRead - byteAvailable: " << readByte;
    if (tempFile.file) {
        tempFile.file->write(reply->readAll());
    }
    emit reply_progress(readByte);
}

void DownloadWorker::doneRead() {
    if (this->tempFile.file) {
        this->tempFile.file->close();
    }
}

void DownloadWorker::cancle_download_slot() {
    this->isCancle = true;
    this->reply->abort();
    qDebug() << "DownloadWorker Cancle download: " << tempFile.file->fileName();
    if (tempFile.file) {
        tempFile.file->close();
        tempFile.file->remove();
        delete tempFile.file;
        tempFile.file = nullptr;
    }
    emit cancle_download_signal();
}

void DownloadWorker::run() {
    QNetworkRequest request(rqParam.url);
    request.setAttribute(QNetworkRequest::HTTP2AllowedAttribute, QVariant(true));
    request.setAttribute(QNetworkRequest::HTTP2WasUsedAttribute, QVariant(true));
    QString concatenated = QStringLiteral("%1:%2").arg(rqParam.user, rqParam.password);
    QByteArray data = concatenated.toLocal8Bit().toBase64();
    QString headerData = "Basic " + data;
    request.setRawHeader("Authorization", headerData.toLocal8Bit());
    QString rangeHeader = QStringLiteral("bytes=%1-%2").arg(rqParam.start).arg(rqParam.end);
    request.setRawHeader("Range", rangeHeader.toLocal8Bit());
    QHttp2Configuration http2Config = request.http2Configuration();
    http2Config.setMaxFrameSize(65536);
    request.setHttp2Configuration(http2Config);
    this->reply = qnam.get(request);
    QEventLoop waitingLoop;
    QObject::connect(reply, &QNetworkReply::finished, &waitingLoop, &QEventLoop::quit);
    //QObject::connect(this, &DownloadWorker::cancle_download_signal, &waitingLoop, &QEventLoop::quit);
    //QObject::connect(reply, &QNetworkReply::downloadProgress, this, &DownloadWorker::reply_progress);

    QObject::connect(reply, &QNetworkReply::readyRead, this, &DownloadWorker::readyRead);
    waitingLoop.exec();
    if (!this->isCancle) {
        qDebug() << "Download done: " << tempFile.file->fileName();
        emit download_done(tempFile.file->fileName());
    }
}

HttpWindow::HttpWindow(QWidget *parent)
    : QDialog(parent)
    , statusLabel(new QLabel(tr("Please enter the URL of a file you want to download.\n\n"), this))
    , urlLineEdit(new QLineEdit(defaultUrl))
    , downloadButton(new QPushButton(tr("Download")))
    , launchCheckBox(new QCheckBox("Launch file"))
    , defaultFileLineEdit(new QLineEdit(defaultFileName))
    , downloadDirectoryLineEdit(new QLineEdit)
    , userNameEdit(new QLineEdit)
    , passEdit(new QLineEdit)
    , proxyServerEdit(new QLineEdit)
    , proxyPortEdit(new QLineEdit)
    , qnam(new QNetworkAccessManager)
    , reply(nullptr)
    , file(nullptr)
    , httpRequestAborted(false)
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setWindowTitle(tr("Buffalo-Downloader"));

    connect(qnam, &QNetworkAccessManager::authenticationRequired,
        this, &HttpWindow::slotAuthenticationRequired);
#ifndef QT_NO_SSL
    connect(qnam, &QNetworkAccessManager::sslErrors,
        this, &HttpWindow::sslErrors);
#endif

    QFormLayout *formLayout = new QFormLayout;
    urlLineEdit->setClearButtonEnabled(true);
    connect(urlLineEdit, &QLineEdit::textChanged,
        this, &HttpWindow::enableDownloadButton);
    formLayout->addRow(tr("&URL:"), urlLineEdit);
    QString downloadDirectory = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (downloadDirectory.isEmpty() || !QFileInfo(downloadDirectory).isDir())
        downloadDirectory = QDir::currentPath();
    downloadDirectoryLineEdit->setText(QDir::toNativeSeparators(downloadDirectory));
    formLayout->addRow(tr("&Download directory:"), downloadDirectoryLineEdit);
    formLayout->addRow(tr("Default &file:"), defaultFileLineEdit);
    formLayout->addRow(tr("User"), userNameEdit);
    formLayout->addRow(tr("Password"), passEdit);
    passEdit->setEchoMode(QLineEdit::Password);
    formLayout->addRow(tr("Proxy"), proxyServerEdit);
    formLayout->addRow(tr("Proxy port"), proxyPortEdit);
    launchCheckBox->setChecked(false);
    formLayout->addRow(launchCheckBox);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(formLayout);

    mainLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Ignored, QSizePolicy::MinimumExpanding));

    statusLabel->setWordWrap(true);
    mainLayout->addWidget(statusLabel);

    downloadButton->setDefault(true);
    connect(downloadButton, &QAbstractButton::clicked, this, &HttpWindow::downloadFile);
    QPushButton *quitButton = new QPushButton(tr("Quit"));
    quitButton->setAutoDefault(false);
    connect(quitButton, &QAbstractButton::clicked, this, &QWidget::close);
    QDialogButtonBox *buttonBox = new QDialogButtonBox;
    buttonBox->addButton(downloadButton, QDialogButtonBox::ActionRole);
    buttonBox->addButton(quitButton, QDialogButtonBox::RejectRole);
    mainLayout->addWidget(buttonBox);

    urlLineEdit->setFocus();
    QString proxyName = proxyServerEdit->text();
    int port = proxyPortEdit->text().toInt();
    if (!proxyName.isEmpty() && port) {
        QNetworkProxy proxy;
        proxy.setType(QNetworkProxy::HttpProxy);
        proxy.setHostName(proxyName);
        proxy.setPort(port);
        qnam->setProxy(proxy);
    }

    //for (int i = 0; i < 2; i++) {
    //	downloadWorkerPools.push_back(new DownloadWorker());
    //}
    //
    //for (auto worker : downloadWorkerPools) {
    //    worker->start();
    //}

}

quint64 HttpWindow::getContentLength(const QUrl &requestedUrl)
{
    url = requestedUrl;
    httpRequestAborted = false;
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::HTTP2AllowedAttribute, QVariant(true));
    QString user = userNameEdit->text();
    QString password = passEdit->text();
    QString concatenated = QStringLiteral("%1:%2").arg(user, password);
    QByteArray data = concatenated.toLocal8Bit().toBase64();
    QString headerData = "Basic " + data;
    request.setRawHeader("Authorization", headerData.toLocal8Bit());
    QHttp2Configuration http2Config = request.http2Configuration();
    http2Config.setMaxFrameSize(65536);
    request.setHttp2Configuration(http2Config);
    reply = qnam->head(request);
    QEventLoop eventLoop;
    QObject::connect(reply, SIGNAL(finished()), &eventLoop, SLOT(quit()));
    eventLoop.exec();

    QList<QByteArray> reply_headers = reply->rawHeaderList();
    quint64 len = 0;
    foreach(QByteArray head, reply_headers) {
        QString headStr = QString::fromUtf8(head);
        if (headStr.contains(CONTENT_LENGTH, Qt::CaseInsensitive)) {
            len = reply->rawHeader(head).toULong();
        }
        qDebug() << head << ":" << reply->rawHeader(head);
    }
    return len;
}

DownloadWorker*  HttpWindow::pickWorker() {
    for (auto worker: downloadWorkerPools) {
        if (worker->isIdle()) {
            return worker;
        }
    }

    return nullptr;
}

void HttpWindow::startRequest(const QUrl &requestedUrl)
{
    totalBytes = getContentLength(requestedUrl);
    currentBytes = 0;
    url = requestedUrl;
    httpRequestAborted = false;
    QString user = userNameEdit->text();
    QString password = passEdit->text();
    //QNetworkRequest request(url);
    //request.setAttribute(QNetworkRequest::HTTP2AllowedAttribute, QVariant(true));
    //request.setAttribute(QNetworkRequest::HTTP2WasUsedAttribute, QVariant(true));
    //QHttp2Configuration http2Config = request.http2Configuration();

    //QString concatenated = QStringLiteral("%1:%2").arg(user, password);
    //QByteArray data = concatenated.toLocal8Bit().toBase64();
    //QString headerData = "Basic " + data;
    //request.setRawHeader("Authorization", headerData.toLocal8Bit());
    //reply = qnam->head(request);
    //connect(reply, &QNetworkReply::finished, this, &HttpWindow::httpFinished);
    //connect(reply, &QIODevice::readyRead, this, &HttpWindow::httpReadyRead);

    ProgressDialog *progressDialog = new ProgressDialog(url, this);
    progressDialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(progressDialog, &QProgressDialog::canceled, this, &HttpWindow::cancelDownload);
    connect(this, &HttpWindow::download_progress_signal, progressDialog, &ProgressDialog::networkReplyProgress);
    connect(this, &HttpWindow::download_done_signal, progressDialog, &ProgressDialog::hide);
    connect(this, &HttpWindow::cancle_signal, progressDialog, &ProgressDialog::close);
    //connect(this, &HttpWindow::cancle_signal, progressDialog, &ProgressDialog::hide);
    progressDialog->show();

    statusLabel->setText(tr("Downloading %1...").arg(url.toString()));

    //Testing
    REQUEST_PARAM rqParam;
    rqParam.url = requestedUrl;
    rqParam.user = user;
    rqParam.password = password;
    QString proxyName = proxyServerEdit->text();
    int port = proxyPortEdit->text().toInt();
    rqParam.proxyName = proxyName;
    rqParam.proxyPort = port;

    int size = filePools.size();
    quint64 step = totalBytes / size;
    quint64 start = 0;
    quint64 end = step;
    for (auto item : filePools) {
        rqParam.start = start;
        rqParam.end = end;

        DownloadWorker *worker = new DownloadWorker(rqParam, item);
        connect(worker, &DownloadWorker::download_done, this, &HttpWindow::rebuildFile);
        connect(this, &HttpWindow::cancle_signal, worker, &DownloadWorker::cancle_download_slot);
        connect(worker, &DownloadWorker::reply_progress, this, &HttpWindow::download_progress);
        connect(worker, &QThread::finished, worker, &QThread::deleteLater, Qt::QueuedConnection);

        worker->start();
        start = end + 1;
        end = start + step;
    }
}

void HttpWindow::downloadFile()
{
    const QString urlSpec = urlLineEdit->text().trimmed();
    if (urlSpec.isEmpty())
        return;

    const QUrl newUrl = QUrl::fromUserInput(urlSpec);
    if (!newUrl.isValid()) {
        QMessageBox::information(this, tr("Error"),
            tr("Invalid URL: %1: %2").arg(urlSpec, newUrl.errorString()));
        return;
    }

    QString fileName = newUrl.fileName();
    if (fileName.isEmpty())
        fileName = defaultFileLineEdit->text().trimmed();
    if (fileName.isEmpty())
        fileName = defaultFileName;
    QString downloadDirectory = QDir::cleanPath(downloadDirectoryLineEdit->text().trimmed());
    bool useDirectory = !downloadDirectory.isEmpty() && QFileInfo(downloadDirectory).isDir();
    if (useDirectory)
        fileName.prepend(downloadDirectory + '/');
    if (QFile::exists(fileName)) {
        if (QMessageBox::question(this, tr("Overwrite Existing File"),
            tr("There already exists a file called %1%2."
                " Overwrite?")
            .arg(fileName,
                useDirectory
                ? QString()
                : QStringLiteral(" in the current directory")),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No)
            == QMessageBox::No) {
            return;
        }
        QFile::remove(fileName);
    }

    file = openFileForWrite(fileName);
    filePools.clear();
    for (int i = 0 ; i < 20; i++) {
        QString tempFileName = fileName;
        tempFileName.append(QStringLiteral("_%1").arg(i));
        filePools.push_back(TEMP_FILE(openFileForWrite(tempFileName)));
    }
    if (!file)
        return;

    downloadButton->setEnabled(false);

    // schedule the request
    startRequest(newUrl);
}

QFile *HttpWindow::openFileForWrite(const QString &fileName)
{
    QScopedPointer<QFile> file(new QFile(fileName));
    if (!file->open(QIODevice::WriteOnly)) {
        QMessageBox::information(this, tr("Error"),
            tr("Unable to save the file %1: %2.")
            .arg(QDir::toNativeSeparators(fileName),
                file->errorString()));
        return nullptr;
    }
    return file.take();
}

QFile *HttpWindow::openFileForRead(const QString &fileName) {
    QScopedPointer<QFile> file(new QFile(fileName));
    if (!file->open(QIODevice::ReadOnly)) {
        QMessageBox::information(this, tr("Error"),
            tr("Unable to save the file %1: %2.")
            .arg(QDir::toNativeSeparators(fileName),
                file->errorString()));
        return nullptr;
    }
    return file.take();
}

void HttpWindow::cancelDownload()
{
    if (this->httpRequestAborted) {
        qDebug() << "The request is already cancled";
        return;
    }
    qDebug() << "Cancle download: " << this->file->fileName();
    statusLabel->setText(tr("Download canceled."));
    httpRequestAborted = true;
    //reply->abort();
    downloadButton->setEnabled(true);
    if (this->file) {
        file->close();
        file->remove();
        delete file;
        file = nullptr;
    }
    emit cancle_signal();
}

void HttpWindow::rebuildFile(const QString &fileName) {
    qDebug() << "Rebuild file";
    int count = 0;
    int size = filePools.size();
    for (auto &item : filePools) {
        if (item.file->fileName() == fileName) {
            item.isFinished = true;
        }
        if (item.isFinished) {
            count++;
        }
    }
    if (count == size) {
        for (auto item : filePools) {
                item.file->flush();
                item.file->close();
                QString fileNameTemp = item.file->fileName();
                auto tempFile = openFileForRead(fileNameTemp);
                this->file->write(tempFile->readAll());
                tempFile->close();
                delete tempFile;
                item.file->remove();
                delete item.file;
        }
        filePools.clear();
        file->close();
        delete file;
        file = nullptr;
        emit download_done_signal();
    }
}

void HttpWindow::httpFinished()
{
    QFileInfo fi;
    fi.setFile(file->fileName());
    //if (file) {
    //	fi.setFile(file->fileName());
    //	file->close();
    //	delete file;
    //	file = nullptr;
    //}

    if (httpRequestAborted) {
        reply->deleteLater();
        reply = nullptr;
        return;
    }

    if (reply->error()) {
        QFile::remove(fi.absoluteFilePath());
        statusLabel->setText(tr("Download failed:\n%1.").arg(reply->errorString()));
        downloadButton->setEnabled(true);
        reply->deleteLater();
        reply = nullptr;
        return;
    }

    const QVariant redirectionTarget = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);

    QList<QByteArray> reply_headers = reply->rawHeaderList();

    foreach(QByteArray head, reply_headers) {
        qDebug() << head << ":" << reply->rawHeader(head);
    }

    reply->deleteLater();
    reply = nullptr;

    if (!redirectionTarget.isNull()) {
        const QUrl redirectedUrl = url.resolved(redirectionTarget.toUrl());
        if (QMessageBox::question(this, tr("Redirect"),
            tr("Redirect to %1 ?").arg(redirectedUrl.toString()),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::No) {
            QFile::remove(fi.absoluteFilePath());
            downloadButton->setEnabled(true);
            statusLabel->setText(tr("Download failed:\nRedirect rejected."));
            return;
        }
        file = openFileForWrite(fi.absoluteFilePath());
        if (!file) {
            downloadButton->setEnabled(true);
            return;
        }
        startRequest(redirectedUrl);
        return;
    }

    statusLabel->setText(tr("Downloaded %1 bytes to %2\nin\n%3")
        .arg(fi.size()).arg(fi.fileName(), QDir::toNativeSeparators(fi.absolutePath())));
    if (launchCheckBox->isChecked())
        QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absoluteFilePath()));
    downloadButton->setEnabled(true);
}

void HttpWindow::httpReadyRead()
{
    // this slot gets called every time the QNetworkReply has new data.
    // We read all of its new data and write it into the file.
    // That way we use less RAM than when reading it at the finished()
    // signal of the QNetworkReply
    if (file)
        file->write(reply->readAll());
}

void HttpWindow::enableDownloadButton()
{
    downloadButton->setEnabled(!urlLineEdit->text().isEmpty());
}

void HttpWindow::slotAuthenticationRequired(QNetworkReply *, QAuthenticator *authenticator)
{
    QDialog authenticationDialog;
    Ui::Dialog ui;
    ui.setupUi(&authenticationDialog);
    authenticationDialog.adjustSize();
    ui.siteDescription->setText(tr("%1 at %2").arg(authenticator->realm(), url.host()));

    // Did the URL have information? Fill the UI
    // This is only relevant if the URL-supplied credentials were wrong
    ui.userEdit->setText(url.userName());
    ui.passwordEdit->setText(url.password());

    if (authenticationDialog.exec() == QDialog::Accepted) {
        authenticator->setUser(ui.userEdit->text());
        authenticator->setPassword(ui.passwordEdit->text());
    }
}

void HttpWindow::download_progress(qint64 bytesRead) {
   qDebug() << "HttpWindow download_progress " << bytesRead;
   if (this->httpRequestAborted) {
       qDebug() << "HttpWindow download_progress: Request is cancled";
       return;
   }
   currentBytes += bytesRead;
   emit download_progress_signal(currentBytes, this->totalBytes);
}

#ifndef QT_NO_SSL
void HttpWindow::sslErrors(QNetworkReply *, const QList<QSslError> &errors)
{
    QString errorString;
    foreach(const QSslError &error, errors) {
        if (!errorString.isEmpty())
            errorString += '\n';
        errorString += error.errorString();
    }

    if (QMessageBox::warning(this, tr("SSL Errors"),
        tr("One or more SSL errors has occurred:\n%1").arg(errorString),
        QMessageBox::Ignore | QMessageBox::Abort) == QMessageBox::Ignore) {
        reply->ignoreSslErrors();
    }
}
#endif
