package com.deskbuddy.app

import android.Manifest
import android.content.ComponentName
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.provider.Settings
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import com.deskbuddy.app.ui.Actions
import com.deskbuddy.app.ui.DeskBuddyApp

class MainActivity : ComponentActivity() {
    private lateinit var prefs: Prefs
    private lateinit var tts: TtsPcm
    private lateinit var discovery: Discovery

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        prefs = Prefs(this)
        tts = TtsPcm(this)
        discovery = Discovery(this)
        AppState.load(prefs)

        if (Build.VERSION.SDK_INT >= 33 &&
            checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(arrayOf(Manifest.permission.POST_NOTIFICATIONS), 1)
        }
        // reconnect automatically if the user linked last time
        if (prefs.linkOn) LinkService.start(this)

        val activity = this
        val actions = object : Actions {
            override val host get() = prefs.host
            override val port get() = prefs.port
            override var forward: Boolean
                get() = prefs.forwardNotifications
                set(v) { prefs.forwardNotifications = v }
            override var echo: Boolean
                get() = prefs.echoVoice
                set(v) { prefs.echoVoice = v }

            override fun link(host: String, port: Int) {
                prefs.host = host
                prefs.port = port
                prefs.linkOn = true
                LinkService.start(activity)         // connects (or reconnects to a new host/port)
            }

            override fun stop() {
                prefs.linkOn = false
                LinkService.stop(activity)
            }

            override fun find(onFound: (String, Int) -> Unit) {
                discovery.find(onFound) { msg -> toast(msg) }
            }

            override fun openNotificationAccess() {
                startActivity(Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS))
            }

            override fun speak(text: String) {
                if (!DeskBuddyClient.connected) toast("Not linked to DeskBuddy") else tts.speak(text)
            }

            override fun setTheme(theme: String) {
                prefs.theme = theme
                AppState.theme = theme
            }

            override fun setBlocked(app: String, blocked: Boolean) = AppState.setBlocked(prefs, app, blocked)
        }

        setContent { DeskBuddyApp(actions) }
    }

    override fun onResume() {
        super.onResume()
        AppState.notifAccess = notificationAccessGranted()
    }

    override fun onDestroy() {
        discovery.stop()
        tts.shutdown()
        super.onDestroy()
    }

    private fun toast(msg: String) = runOnUiThread { Toast.makeText(this, msg, Toast.LENGTH_SHORT).show() }

    private fun notificationAccessGranted(): Boolean {
        val enabled = Settings.Secure.getString(contentResolver, "enabled_notification_listeners") ?: return false
        return enabled.contains(ComponentName(this, NotifListener::class.java).flattenToString())
    }
}
