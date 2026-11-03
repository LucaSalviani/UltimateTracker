#include "AndroidBridge.h"

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QJniObject>

namespace {
const char* kClass = "com/lucasalviani/ultimatetracker/Notifier";

QJniObject context()
{
    return QJniObject(QNativeInterface::QAndroidApplication::context());
}

QJniObject jstr(const QString& s) { return QJniObject::fromString(s); }
} // namespace

AndroidBridge::AndroidBridge()
{
    QJniObject::callStaticMethod<void>(kClass, "init", "(Landroid/content/Context;)V", context().object());
}

void AndroidBridge::requestPermissions()
{
    QJniObject::callStaticMethod<void>(kClass, "requestPermissions", "(Landroid/content/Context;)V", context().object());
}

void AndroidBridge::showRunning(const QString& title, const QString& text, qint64 endEpochMs, bool isBreak)
{
    QJniObject::callStaticMethod<void>(kClass, "showRunning",
        "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;JZ)V",
        context().object(), jstr(title).object<jstring>(), jstr(text).object<jstring>(),
        jlong(endEpochMs), jboolean(isBreak));
}

void AndroidBridge::scheduleEnd(qint64 atEpochMs, const QString& title, const QString& text)
{
    QJniObject::callStaticMethod<void>(kClass, "scheduleEnd",
        "(Landroid/content/Context;JLjava/lang/String;Ljava/lang/String;)V",
        context().object(), jlong(atEpochMs), jstr(title).object<jstring>(), jstr(text).object<jstring>());
}

void AndroidBridge::cancelEnd()
{
    QJniObject::callStaticMethod<void>(kClass, "cancelEnd", "(Landroid/content/Context;)V", context().object());
}

void AndroidBridge::stopRunning()
{
    QJniObject::callStaticMethod<void>(kClass, "stopRunning", "(Landroid/content/Context;)V", context().object());
}

void AndroidBridge::vibrate(int ms)
{
    QJniObject::callStaticMethod<void>(kClass, "vibrate", "(Landroid/content/Context;I)V", context().object(), jint(ms));
}

#else
AndroidBridge::AndroidBridge() {}
void AndroidBridge::requestPermissions() {}
void AndroidBridge::showRunning(const QString&, const QString&, qint64, bool) {}
void AndroidBridge::scheduleEnd(qint64, const QString&, const QString&) {}
void AndroidBridge::cancelEnd() {}
void AndroidBridge::stopRunning() {}
void AndroidBridge::vibrate(int) {}
#endif
