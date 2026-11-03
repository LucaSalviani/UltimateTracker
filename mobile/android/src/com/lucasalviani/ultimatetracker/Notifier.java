package com.lucasalviani.ultimatetracker;

import android.app.Activity;
import android.app.AlarmManager;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.media.AudioAttributes;
import android.media.RingtoneManager;
import android.net.Uri;
import android.os.Build;
import android.os.VibrationEffect;
import android.os.Vibrator;

// Lo llama la app (C++) por JNI: canales, permisos, notificación en curso y alarma del final.
public class Notifier {
    static final String CH_RUN = "ut_run";
    static final String CH_END = "ut_end";
    static final int ID_RUN = 1;
    static final int ID_END = 2;

    public static void init(Context c) {
        NotificationManager nm = c.getSystemService(NotificationManager.class);
        if (nm == null) return;
        NotificationChannel run = new NotificationChannel(CH_RUN, "Pomodoro en curso", NotificationManager.IMPORTANCE_LOW);
        run.setShowBadge(false);
        nm.createNotificationChannel(run);
        NotificationChannel end = new NotificationChannel(CH_END, "Fin del pomodoro y del recreo", NotificationManager.IMPORTANCE_HIGH);
        end.enableVibration(true);
        end.setVibrationPattern(new long[]{0, 400, 200, 400});
        Uri sound = RingtoneManager.getDefaultUri(RingtoneManager.TYPE_NOTIFICATION);
        end.setSound(sound, new AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_ALARM)
                .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
                .build());
        nm.createNotificationChannel(end);
    }

    public static void requestPermissions(Context c) {
        if (Build.VERSION.SDK_INT >= 33 && c instanceof Activity) {
            ((Activity) c).requestPermissions(new String[]{"android.permission.POST_NOTIFICATIONS"}, 4401);
        }
    }

    static PendingIntent openApp(Context c) {
        Intent i = c.getPackageManager().getLaunchIntentForPackage(c.getPackageName());
        if (i == null) i = new Intent();
        i.setFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_CLEAR_TOP);
        return PendingIntent.getActivity(c, 0, i, PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
    }

    public static void showRunning(Context c, String title, String text, long endMs, boolean isBreak) {
        Intent i = new Intent(c, TimerService.class);
        i.putExtra("title", title);
        i.putExtra("text", text);
        i.putExtra("end", endMs);
        i.putExtra("break", isBreak);
        try {
            c.startForegroundService(i);
        } catch (Exception e) {
            // Si Android no deja arrancar el servicio (app en segundo plano), al menos la notificación.
            TimerService.post(c, title, text, endMs, isBreak);
        }
    }

    public static void stopRunning(Context c) {
        c.stopService(new Intent(c, TimerService.class));
        NotificationManager nm = c.getSystemService(NotificationManager.class);
        if (nm != null) nm.cancel(ID_RUN);
    }

    static PendingIntent alarmIntent(Context c, String title, String text) {
        Intent i = new Intent(c, AlarmReceiver.class);
        if (title != null) i.putExtra("title", title);
        if (text != null) i.putExtra("text", text);
        return PendingIntent.getBroadcast(c, 7, i, PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
    }

    public static void scheduleEnd(Context c, long atMs, String title, String text) {
        AlarmManager am = c.getSystemService(AlarmManager.class);
        if (am == null) return;
        PendingIntent pi = alarmIntent(c, title, text);
        if (Build.VERSION.SDK_INT >= 31 && !am.canScheduleExactAlarms()) {
            am.setAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, atMs, pi);
        } else {
            am.setExactAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, atMs, pi);
        }
    }

    public static void cancelEnd(Context c) {
        AlarmManager am = c.getSystemService(AlarmManager.class);
        if (am != null) am.cancel(alarmIntent(c, null, null));
    }

    public static void vibrate(Context c, int ms) {
        Vibrator v = c.getSystemService(Vibrator.class);
        if (v != null && v.hasVibrator()) v.vibrate(VibrationEffect.createOneShot(ms, VibrationEffect.DEFAULT_AMPLITUDE));
    }
}
