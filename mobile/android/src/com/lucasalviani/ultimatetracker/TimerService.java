package com.lucasalviani.ultimatetracker;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.os.IBinder;

// Servicio en primer plano mientras corre un pomodoro o un recreo: muestra la cuenta
// regresiva en la notificación y evita que Android cierre la app con la pantalla apagada.
public class TimerService extends Service {
    static Notification build(Context c, String title, String text, long endMs, boolean isBreak) {
        Notification.Builder b = new Notification.Builder(c, Notifier.CH_RUN)
                .setSmallIcon(c.getApplicationInfo().icon)
                .setContentTitle(title)
                .setContentText(text)
                .setOngoing(true)
                .setOnlyAlertOnce(true)
                .setColor(isBreak ? 0xFF3CB89B : 0xFFE0483F)
                .setCategory(Notification.CATEGORY_PROGRESS)
                .setContentIntent(Notifier.openApp(c));
        if (endMs > 0) {
            b.setWhen(endMs).setShowWhen(true).setUsesChronometer(true).setChronometerCountDown(true);
        }
        return b.build();
    }

    static void post(Context c, String title, String text, long endMs, boolean isBreak) {
        NotificationManager nm = c.getSystemService(NotificationManager.class);
        if (nm != null) nm.notify(Notifier.ID_RUN, build(c, title, text, endMs, isBreak));
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        String title = intent != null ? intent.getStringExtra("title") : "UltimateTracker";
        String text = intent != null ? intent.getStringExtra("text") : "";
        long end = intent != null ? intent.getLongExtra("end", 0) : 0;
        boolean brk = intent != null && intent.getBooleanExtra("break", false);
        Notification n = build(this, title, text, end, brk);
        if (Build.VERSION.SDK_INT >= 34) {
            startForeground(Notifier.ID_RUN, n, ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE);
        } else {
            startForeground(Notifier.ID_RUN, n);
        }
        return START_NOT_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) { return null; }
}
