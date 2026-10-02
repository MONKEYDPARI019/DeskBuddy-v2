package com.deskbuddy.app

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.IBinder

/**
 * Foreground service that keeps the DeskBuddy connection (and voice loop)
 * running while the app is in the background.
 */
class LinkService : Service(), DeskBuddyClient.Listener {
    private var voice: VoiceLoop? = null

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_STOP) {
            DeskBuddyClient.disconnect()
            stopForeground(STOP_FOREGROUND_REMOVE)
            stopSelf()
            return START_NOT_STICKY
        }
        startForegroundCompat("Connecting…")

        val prefs = Prefs(this)
        if (voice == null) {
            voice = VoiceLoop(this, prefs).also { DeskBuddyClient.addListener(it) }
            DeskBuddyClient.addListener(this)
        }
        DeskBuddyClient.connect(prefs.host, prefs.port)
        return START_STICKY
    }

    override fun onStatus(connected: Boolean, text: String) {
        val nm = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        nm.notify(NOTIF_ID, buildNotification(text))
    }

    override fun onDestroy() {
        voice?.let { DeskBuddyClient.removeListener(it); it.shutdown() }
        DeskBuddyClient.removeListener(this)
        voice = null
        super.onDestroy()
    }

    private fun startForegroundCompat(text: String) {
        val n = buildNotification(text)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            startForeground(NOTIF_ID, n, ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE)
        } else {
            startForeground(NOTIF_ID, n)
        }
    }

    private fun buildNotification(text: String): Notification {
        val nm = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        if (nm.getNotificationChannel(CHANNEL) == null) {
            nm.createNotificationChannel(NotificationChannel(CHANNEL, "DeskBuddy link", NotificationManager.IMPORTANCE_LOW))
        }
        val open = PendingIntent.getActivity(
            this, 0, Intent(this, MainActivity::class.java), PendingIntent.FLAG_IMMUTABLE
        )
        return Notification.Builder(this, CHANNEL)
            .setSmallIcon(android.R.drawable.stat_sys_data_bluetooth)
            .setContentTitle("DeskBuddy")
            .setContentText(text)
            .setContentIntent(open)
            .setOngoing(true)
            .build()
    }

    companion object {
        const val ACTION_STOP = "com.deskbuddy.app.STOP"
        private const val CHANNEL = "link"
        private const val NOTIF_ID = 1

        fun start(context: Context) {
            context.startForegroundService(Intent(context, LinkService::class.java))
        }

        fun stop(context: Context) {
            context.startService(Intent(context, LinkService::class.java).setAction(ACTION_STOP))
        }
    }
}
