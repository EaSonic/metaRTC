//
// Copyright (c) 2019-2022 yanggaofeng
//
#include "mainwindow.h"

#include <QApplication>
#include <QDir>
#include <QString>
#include <yangutil/sys/YangLog.h>
#if defined(__APPLE__) || defined(__unix__)
#include <signal.h>
#include <execinfo.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#if defined(__APPLE__) || defined(__unix__)
static int g_crash_fd = -1;
static void installCrashHandlers();
static void crashHandler(int sig);
#endif

int main(int argc, char *argv[])
{
#if defined (__APPLE__)
    QSurfaceFormat format;
    format.setVersion(4,1);
    format.setProfile(QSurfaceFormat::CoreProfile);
   QSurfaceFormat::setDefaultFormat(format);
#endif
    QApplication a(argc, argv);

    // Configure per-app log file under the .app/Contents/MacOS directory
    {
        QString appDir = QCoreApplication::applicationDirPath();
        QString logPath = appDir + "/metaplayer7.log";
        static std::string s_logPath = logPath.toUtf8().constData();
        yang_setLogLevel(5);
        yang_setLogFile2(1, (char*)s_logPath.c_str());
    }

#if defined(__APPLE__) || defined(__unix__)
    installCrashHandlers();
#endif
    MainWindow w;
    YangRecordThread videoThread;

    w.m_videoThread=&videoThread;
    w.initVideoThread(&videoThread);
    w.show();
    videoThread.start();
    return a.exec();
}

#if defined(__APPLE__) || defined(__unix__)
static void installCrashHandlers(){
    QString appDir = QCoreApplication::applicationDirPath();
    QString crashPath = appDir + "/crash.log";
    QByteArray cpath = crashPath.toUtf8();
    g_crash_fd = ::open(cpath.constData(), O_CREAT | O_WRONLY | O_TRUNC, 0644);

    struct sigaction sa; memset(&sa, 0, sizeof(sa));
    sa.sa_handler = crashHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESETHAND;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGBUS,  &sa, NULL);
    sigaction(SIGILL,  &sa, NULL);
    sigaction(SIGFPE,  &sa, NULL);
}

static void crashHandler(int sig){
    void* array[64];
    int size = backtrace(array, 64);
    if (g_crash_fd >= 0) {
        const char header[] = "===== Crash detected =====\n";
        ::write(g_crash_fd, header, sizeof(header)-1);
        backtrace_symbols_fd(array, size, g_crash_fd);
        const char footer[] = "\n==========================\n";
        ::write(g_crash_fd, footer, sizeof(footer)-1);
        ::fsync(g_crash_fd);
    }
    signal(sig, SIG_DFL);
    raise(sig);
}
#endif
