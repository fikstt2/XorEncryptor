#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QRadioButton>
#include <QSpinBox>
#include <QProgressBar>
#include <QTextEdit>
#include <QTimer>
#include "xorworker.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onSelectSrcDir();
    void onSelectDstDir();
    void onStartProcessing();
    void onPauseProcessing();
    void onCancelProcessing();
    void onTimerTick();

    void onWorkerProgress(int percentage);
    void onWorkerMessage(const QString &message);
    void onWorkerSuccess();
    void onWorkerError(const QString &error);

private:
    void setupUi();
    void checkFolderForFiles();

    QLineEdit *lineSrcDir;
    QLineEdit *lineDstDir;
    QLineEdit *lineMask;
    QLineEdit *lineHexKey;

    QPushButton *btnSrcDir;
    QPushButton *btnDstDir;
    QPushButton *btnStart;
    QPushButton *btnPause;
    QPushButton *btnCancel;

    QCheckBox *checkDeleteSource;
    QComboBox *comboConflictMode;
    QRadioButton *radioOnce;
    QRadioButton *radioTimer;
    QSpinBox *spinPeriod;

    QProgressBar *progressBar;
    QTextEdit *textLog;

    QTimer *m_pollingTimer;
    XorWorker *m_worker;
    bool m_isWorking;

    QSet<QString> m_processedFiles; // Храним полные пути уже заксоренных файлов
};

#endif // MAINWINDOW_H