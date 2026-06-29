#ifndef XORWORKER_H
#define XORWORKER_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QString>

class XorWorker : public QThread {
    Q_OBJECT

public:
    explicit XorWorker(QObject *parent = nullptr);

    // Метод для передачи настроек из UI в поток перед запуском
    void setTask(const QString &srcPath, const QString &dstPath,
                 quint64 hexKey, bool deleteSource, int conflictMode);

    // кнопки управления для UI
    void pause();
    void resume();
    void cancel();

signals:
    void progressChanged(int percentage);
    void statusMessage(const QString &message);
    void finishedSuccessfully();
    void errorOccurred(const QString &error);

protected:
    void run() override;

private:
    // входные параметры
    QString m_srcPath;
    QString m_dstPath;
    quint64 m_hexKey;
    bool m_deleteSource;
    int m_conflictMode;

    // инструменты управления паузой и остановкой
    QMutex m_mutex;
    QWaitCondition m_pauseCondition;
    bool m_isPaused;
    bool m_isCancelled;
};

#endif // XORWORKER_H