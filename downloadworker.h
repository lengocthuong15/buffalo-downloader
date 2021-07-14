#ifndef DOWNLOADWORKER_H
#define DOWNLOADWORKER_H

#endif // DOWNLOADWORKER_H


#include <functional>
#include <QThread>
#include <QSharedPointer>
#include <QFile>
#include <QUrl>

class DownloadWorker : public QThread
{
    Q_OBJECT

    public:
        DownloadWorker(const QUrl &requestedUrl, QFile *file, std::function<void(const QUrl&, QFile*)> func)
            : requestedUrl(requestedUrl),
              file(file),
              func(func)
        {}

        void run()
        {
            func(requestedUrl, file);
        }

    protected:
        QUrl requestedUrl;
        QFile *file;
        std::function<void(const QUrl&, QFile*)> func;
};

