package com.deskbuddy.app

import android.content.Context

/** Small wrapper around SharedPreferences for the few things the app remembers. */
class Prefs(context: Context) {
    private val sp = context.getSharedPreferences("deskbuddy", Context.MODE_PRIVATE)

    var host: String
        get() = sp.getString("host", "deskbuddy.local") ?: "deskbuddy.local"
        set(v) = sp.edit().putString("host", v).apply()

    var port: Int
        get() = sp.getInt("port", 81)
        set(v) = sp.edit().putInt("port", v).apply()

    /** True after the user tapped LINK; the app reconnects on launch until they tap STOP. */
    var linkOn: Boolean
        get() = sp.getBoolean("link_on", false)
        set(v) = sp.edit().putBoolean("link_on", v).apply()

    /** Forward phone notifications to DeskBuddy. */
    var forwardNotifications: Boolean
        get() = sp.getBoolean("forward", true)
        set(v) = sp.edit().putBoolean("forward", v).apply()

    /** Voice test mode: play the recorded push-to-talk audio straight back. */
    var echoVoice: Boolean
        get() = sp.getBoolean("echo", true)
        set(v) = sp.edit().putBoolean("echo", v).apply()

    /** "auto" (follow the phone), "light" or "dark". */
    var theme: String
        get() = sp.getString("theme", "auto") ?: "auto"
        set(v) = sp.edit().putString("theme", v).apply()

    /** Apps that have posted a notification at least once (shown in the Inbox filter). */
    var knownApps: Set<String>
        get() = sp.getStringSet("known_apps", emptySet())?.toSet() ?: emptySet()
        set(v) = sp.edit().putStringSet("known_apps", v).apply()

    /** Apps whose notifications are NOT forwarded. */
    var blockedApps: Set<String>
        get() = sp.getStringSet("blocked_apps", emptySet())?.toSet() ?: emptySet()
        set(v) = sp.edit().putStringSet("blocked_apps", v).apply()
}
