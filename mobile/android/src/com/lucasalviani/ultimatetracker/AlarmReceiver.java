package com.lucasalviani.ultimatetracker;

import android.app.Notification;
import android.app.NotificationManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

// Suena cuando termina el pomodoro o el recreo, aunque la app esté dormida.
public class AlarmReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context c, Intent intent) {
        String title = intent.getStringExtra("title");
        String text = intent.getStringExtra("text");
        Notification n = new Notification.Builder(c, Notifier.CH_END)
                .setSmallIcon(c.getApplicationInfo().icon)
                .setContentTitle(title != null ? title : "¡Pomodoro terminado!")
                .setContentText(text != null ? text : "")
                .setAutoCancel(true)
                .setCategory(Notification.CATEGORY_ALARM)
                .setColor(0xFFE0483F)
                .setContentIntent(Notifier.openApp(c))
                .build();
        NotificationManager nm = c.getSystemService(NotificationManager.class);
        if (nm != null) nm.notify(Notifier.ID_END, n);
    }
}
