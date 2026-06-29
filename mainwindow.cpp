#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_worker(nullptr), m_isWorking(false) {

    setupUi();

    m_pollingTimer = new QTimer(this);
    connect(m_pollingTimer, &QTimer::timeout, this, &MainWindow::onTimerTick);
}

MainWindow::~MainWindow() {
    if (m_worker && m_worker->isRunning()) {
        m_worker->cancel();
        m_worker->wait(); // Жестко ждем корректного закрытия потока при выходе
    }
}

void MainWindow::setupUi() {
    this->setWindowTitle("XOR Encryptor - Ефимов Е.М.");
    this->resize(600, 450);

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    QFormLayout *formLayout = new QFormLayout();

    // 1. поля с путями
    QHBoxLayout *srcLayout = new QHBoxLayout();
    lineSrcDir = new QLineEdit(this);
    btnSrcDir = new QPushButton("Обзор...", this);
    srcLayout->addWidget(lineSrcDir);
    srcLayout->addWidget(btnSrcDir);

    QHBoxLayout *dstLayout = new QHBoxLayout();
    lineDstDir = new QLineEdit(this);
    btnDstDir = new QPushButton("Обзор...", this);
    dstLayout->addWidget(lineDstDir);
    dstLayout->addWidget(btnDstDir);

    lineMask = new QLineEdit("*.txt", this);
    lineHexKey = new QLineEdit("1234567890ABCDEF", this);
    lineHexKey->setMaxLength(16);

    formLayout->addRow("Папка поиска:", srcLayout);
    formLayout->addRow("Папка сохранения:", dstLayout);
    formLayout->addRow("Маска файлов:", lineMask);
    formLayout->addRow("Ключ XOR (HEX 8 байт):", lineHexKey);

    mainLayout->addLayout(formLayout);

    // 2. чекбоксы Радио-кнопки
    checkDeleteSource = new QCheckBox("Удалять входные файлы после обработки", this);
    mainLayout->addWidget(checkDeleteSource);

    QHBoxLayout *conflictLayout = new QHBoxLayout();
    comboConflictMode = new QComboBox(this);
    comboConflictMode->addItems({"Перезаписать файл", "Добавить счетчик к имени"});
    conflictLayout->addWidget(comboConflictMode);
    mainLayout->addLayout(conflictLayout);

    QHBoxLayout *modeLayout = new QHBoxLayout();
    radioOnce = new QRadioButton("Разовый запуск", this);
    radioOnce->setChecked(true);
    radioTimer = new QRadioButton("Работа по таймеру (сек):", this);
    spinPeriod = new QSpinBox(this);
    spinPeriod->setRange(1, 3600);
    spinPeriod->setValue(5);

    modeLayout->addWidget(radioOnce);
    modeLayout->addSpacing(20);
    modeLayout->addWidget(radioTimer);
    modeLayout->addWidget(spinPeriod);
    mainLayout->addLayout(modeLayout);

    // 3. прогресс и логи
    progressBar = new QProgressBar(this);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    mainLayout->addWidget(progressBar);

    textLog = new QTextEdit(this);
    textLog->setReadOnly(true);
    mainLayout->addWidget(textLog);

    // Кнопки управления
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnStart = new QPushButton("Старт", this);
    btnPause = new QPushButton("Пауза", this);
    btnCancel = new QPushButton("Отмена", this);
    btnPause->setEnabled(false);
    btnCancel->setEnabled(false);

    btnLayout->addWidget(btnStart);
    btnLayout->addWidget(btnPause);
    btnLayout->addWidget(btnCancel);
    mainLayout->addLayout(btnLayout);

    connect(btnSrcDir, &QPushButton::clicked, this, &MainWindow::onSelectSrcDir);
    connect(btnDstDir, &QPushButton::clicked, this, &MainWindow::onSelectDstDir);
    connect(btnStart, &QPushButton::clicked, this, &MainWindow::onStartProcessing);
    connect(btnPause, &QPushButton::clicked, this, &MainWindow::onPauseProcessing);
    connect(btnCancel, &QPushButton::clicked, this, &MainWindow::onCancelProcessing);
}

void MainWindow::onSelectSrcDir() {
    QString dir = QFileDialog::getExistingDirectory(this, "Выберите папку для поиска файлов");
    if (!dir.isEmpty()) lineSrcDir->setText(dir);
}

void MainWindow::onSelectDstDir() {
    QString dir = QFileDialog::getExistingDirectory(this, "Выберите папку для сохранения результатов");
    if (!dir.isEmpty()) lineDstDir->setText(dir);
}

void MainWindow::onStartProcessing() {

    if (lineSrcDir->text().isEmpty() || lineDstDir->text().isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Укажите пути к папкам!");
        return;
    }

    // очищаем историю обработанных файлов при каждом новом ручном запуске,
    // чтобы пользователь мог заново прогнать те же самые файлы, если захочет
    m_processedFiles.clear();

    if (radioTimer->isChecked()) {
        m_pollingTimer->setInterval(spinPeriod->value() * 1000);
        m_pollingTimer->start();
        btnStart->setEnabled(false);
        btnCancel->setEnabled(true);
        textLog->append("Запущен периодический опрос папки по таймеру...");
    } else {
        checkFolderForFiles();
    }
}
// логика поиска
void MainWindow::checkFolderForFiles() {
    if (m_isWorking) return;

    QDir srcDir(lineSrcDir->text());
    QStringList filters;
    filters << (lineMask->text().isEmpty() ? "*.*" : lineMask->text());

    QFileInfoList files = srcDir.entryInfoList(filters, QDir::Files);

    if (files.isEmpty()) {
        if (radioOnce->isChecked()) {
            textLog->append("Файлы по маске не найдены.");
        }
        return;
    }

    QFileInfo fileInfo;
    bool foundNewFile = false;

    for (const QFileInfo &info : files) {
        QString currentPath = info.absoluteFilePath();
        if (!m_processedFiles.contains(currentPath)) {
            fileInfo = info;
            foundNewFile = true;
            break;
        }
    }

    if (!foundNewFile) {
        return;
    }

    QString srcPath = fileInfo.absoluteFilePath();
    QString dstFileName = fileInfo.fileName();
    QString dstPath = lineDstDir->text() + "/" + dstFileName;

    if (QFile::exists(dstPath)) {
        if (comboConflictMode->currentIndex() == 1) { // Счетчик к имени
            int counter = 1;
            QString baseName = fileInfo.baseName();
            QString ext = fileInfo.completeSuffix();
            do {
                dstPath = lineDstDir->text() + "/" + baseName + "_" + QString::number(counter) + "." + ext;
                counter++;
            } while (QFile::exists(dstPath));
        }
    }

    // парсим HEX строку
    bool ok;
    quint64 key = lineHexKey->text().toULongLong(&ok, 16);
    if (!ok) {
        textLog->append("Ошибка: Неверный формат HEX ключа!");
        m_pollingTimer->stop();
        btnStart->setEnabled(true);
        return;
    }

    m_processedFiles.insert(srcPath);

    m_worker = new XorWorker(this);
    m_worker->setTask(srcPath, dstPath, key, checkDeleteSource->isChecked(), comboConflictMode->currentIndex());

    connect(m_worker, &XorWorker::progressChanged, this, &MainWindow::onWorkerProgress);
    connect(m_worker, &XorWorker::statusMessage, this, &MainWindow::onWorkerMessage);
    connect(m_worker, &XorWorker::finishedSuccessfully, this, &MainWindow::onWorkerSuccess);
    connect(m_worker, &XorWorker::errorOccurred, this, &MainWindow::onWorkerError);

    m_isWorking = true;
    btnStart->setEnabled(false);
    btnPause->setEnabled(true);
    btnCancel->setEnabled(true);
    btnPause->setText("Пауза");

    m_worker->start();
}

void MainWindow::onPauseProcessing() {
    if (!m_worker) return;
    if (btnPause->text() == "Пауза") {
        m_worker->pause();
        btnPause->setText("Продолжить");
    } else {
        m_worker->resume();
        btnPause->setText("Пауза");
    }
}

void MainWindow::onCancelProcessing() {
    textLog->append("Остановка процесса и сброс интерфейса...");

    m_pollingTimer->stop();

    if (m_worker) {
        m_worker->cancel();
        m_worker->wait();   // Дожидаемся, пока поток умрет
        m_worker->deleteLater();
        m_worker = nullptr;
    }

    // возвращаем кнопки в рабочее состояние
    m_isWorking = false;
    progressBar->setValue(0);

    btnStart->setEnabled(true);
    btnPause->setEnabled(false);
    btnCancel->setEnabled(false);
    btnPause->setText("Пауза");
}

void MainWindow::onTimerTick() {
    if (!m_isWorking) {
        checkFolderForFiles();
    }
}

void MainWindow::onWorkerProgress(int percentage) {
    progressBar->setValue(percentage);
}

void MainWindow::onWorkerMessage(const QString &message) {
    textLog->append(message);
}

void MainWindow::onWorkerSuccess() {
    textLog->append("Файл успешно обработан.");
    m_isWorking = false;
    m_worker->deleteLater();
    m_worker = nullptr;

    // И для таймера, и для разового запуска сразу пробуем взять СЛЕДУЮЩИЙ файл
    checkFolderForFiles();

    if (!m_isWorking) {
        btnStart->setEnabled(true);
        btnPause->setEnabled(false);
        btnCancel->setEnabled(false);

        if (radioOnce->isChecked()) {
            textLog->append("Все найденные файлы успешно обработаны.");
        }
    }
}

// обработка ошибки
void MainWindow::onWorkerError(const QString &error) {
    textLog->append("ОШИБКА: " + error);
    m_isWorking = false;
    m_pollingTimer->stop();
    if (m_worker) {
        m_worker->deleteLater();
        m_worker = nullptr;
    }
    btnStart->setEnabled(true);
    btnPause->setEnabled(false);
    btnCancel->setEnabled(false);
}