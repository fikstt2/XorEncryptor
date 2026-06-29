#include "xorworker.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>

XorWorker::XorWorker(QObject *parent)
    : QThread(parent), m_isPaused(false), m_isCancelled(false) {}

void XorWorker::setTask(const QString &srcPath, const QString &dstPath,
                        quint64 hexKey, bool deleteSource, int conflictMode) {
    m_srcPath = srcPath;
    m_dstPath = dstPath;
    m_hexKey = hexKey;
    m_deleteSource = deleteSource;
    m_conflictMode = conflictMode;
}

void XorWorker::pause() {
    QMutexLocker locker(&m_mutex);
    m_isPaused = true;
    emit statusMessage("Обработка приостановлена...");
}

void XorWorker::resume() {
    QMutexLocker locker(&m_mutex);
    if (m_isPaused) {
        m_isPaused = false;
        m_pauseCondition.wakeAll(); // Будим спящий поток
        emit statusMessage("Обработка возобновлена.");
    }
}

void XorWorker::cancel() {
    QMutexLocker locker(&m_mutex);
    m_isCancelled = true;
    if (m_isPaused) {
        m_isPaused = false;
        m_pauseCondition.wakeAll(); // Если поток спал, будим его для выхода
    }
}
void XorWorker::run() {
    QFile srcFile(m_srcPath);
    if (!srcFile.open(QIODevice::ReadOnly)) {
        emit errorOccurred("Не удалось открыть исходный файл: " + srcFile.errorString());
        return;
    }

    QFile dstFile(m_dstPath);
    if (!dstFile.open(QIODevice::WriteOnly)) {
        emit errorOccurred("Не удалось создать результирующий файл.");
        srcFile.close();
        return;
    }

    qint64 fileSize = srcFile.size();
    qint64 processedBytes = 0;

    // буфер в 64кб
    const qint64 bufferSize = 65536;
    QByteArray buffer;

    emit statusMessage("Обработка файла началась...");

    while (!srcFile.atEnd()) {
        // проверка на паузу и отмену
        {
            QMutexLocker locker(&m_mutex);
            if (m_isCancelled) {
                srcFile.close();
                dstFile.close();
                dstFile.remove(); // Удаляем недописанный файл
                emit statusMessage("Обработка отменена пользователем.");
                return;
            }

            while (m_isPaused) {
                m_pauseCondition.wait(&m_mutex); // ОС усыпляет поток с 0% нагрузки на CPU
            }
        }

        // читаем блок данных из файла в оперативку
        buffer = srcFile.read(bufferSize);
        qint64 bytesRead = buffer.size();

        // XOR по 8 баит
        int i = 0;
        while (i < bytesRead) {
            if (i + 8 <= bytesRead) {
                quint64 *ptr = reinterpret_cast<quint64*>(buffer.data() + i);
                *ptr ^= m_hexKey;
                i += 8;
            } else {
                // если в конце файла меньше 8 байт — добиваем побайтово
                char *ptr = buffer.data() + i;
                char keyByte = static_cast<char>((m_hexKey >> ((i % 8) * 8)) & 0xFF);
                *ptr ^= keyByte;
                i++;
            }
        }

        // пишем изменённый блок на диск
        dstFile.write(buffer);
        processedBytes += bytesRead;

        // отправляем сигнал прогресса в интерфейс
        if (fileSize > 0) {
            int progress = static_cast<int>((processedBytes * 100) / fileSize);
            emit progressChanged(progress);
        }
    }

    srcFile.close();
    dstFile.close();

    if (m_deleteSource) {
        srcFile.remove();
    }

    emit progressChanged(100);
    emit finishedSuccessfully();
}