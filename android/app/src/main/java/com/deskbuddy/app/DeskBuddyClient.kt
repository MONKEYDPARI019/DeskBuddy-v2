package com.deskbuddy.app

import android.os.Handler
import android.os.Looper
import android.util.Log
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.Response
import okhttp3.WebSocket
import okhttp3.WebSocketListener
import okio.ByteString
import okio.ByteString.Companion.toByteString
import org.json.JSONObject
import java.util.concurrent.CopyOnWriteArrayList
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit

/**
 * One WebSocket connection to the DeskBuddy (see docs/PROTOCOL.md).
 * Reconnects automatically with exponential backoff while [connect] is in effect.
 * Listener callbacks run on OkHttp's background thread.
 */
object DeskBuddyClient {
    private const val TAG = "DeskBuddyClient"
    private const val SPEECH_CHUNK_MS = 64

    interface Listener {
        fun onStatus(connected: Boolean, text: String) {}
        fun onJson(msg: JSONObject) {}
        fun onAudio(pcm: ByteArray) {}
    }

    private val http = OkHttpClient.Builder()
        .pingInterval(10, TimeUnit.SECONDS)
        .readTimeout(0, TimeUnit.MILLISECONDS)
        .build()
    private val handler = Handler(Looper.getMainLooper())
    private val speechExecutor = Executors.newSingleThreadExecutor()
    private val listeners = CopyOnWriteArrayList<Listener>()

    @Volatile private var ws: WebSocket? = null
    @Volatile var connected = false
        private set
    @Volatile var lastState: JSONObject? = null
        private set
    @Volatile var statusText = "disconnected"
        private set

    private var url: String? = null
    private var wanted = false
    private var backoffMs = 1000L

    init {
        listeners.add(AppState)   // the UI state always listens
    }

    fun addListener(l: Listener) = listeners.addIfAbsent(l)
    fun removeListener(l: Listener) = listeners.remove(l)

    @Synchronized
    fun connect(host: String, port: Int) {
        val newUrl = "ws://$host:$port/"
        if (wanted && newUrl == url && ws != null) return
        ws?.cancel()
        url = newUrl
        wanted = true
        backoffMs = 1000L
        open()
    }

    @Synchronized
    fun disconnect() {
        wanted = false
        handler.removeCallbacksAndMessages(null)
        ws?.close(1000, "bye")
        ws = null
        setStatus(false, "disconnected")
    }

    private fun open() {
        val u = url ?: return
        setStatus(false, "connecting to $u")
        ws = http.newWebSocket(Request.Builder().url(u).build(), object : WebSocketListener() {
            override fun onOpen(webSocket: WebSocket, response: Response) {
                backoffMs = 1000L
                setStatus(true, "connected to $u")
                send(JSONObject().put("type", "hello").put("name", android.os.Build.MODEL))
            }

            override fun onMessage(webSocket: WebSocket, text: String) {
                val msg = try { JSONObject(text) } catch (e: Exception) { return }
                if (msg.optString("type") == "state") lastState = msg
                listeners.forEach { it.onJson(msg) }
            }

            override fun onMessage(webSocket: WebSocket, bytes: ByteString) {
                val pcm = bytes.toByteArray()
                listeners.forEach { it.onAudio(pcm) }
            }

            override fun onClosed(webSocket: WebSocket, code: Int, reason: String) = lost(webSocket, "closed")

            override fun onFailure(webSocket: WebSocket, t: Throwable, response: Response?) =
                lost(webSocket, t.message ?: "failed")
        })
    }

    private fun lost(socket: WebSocket, why: String) {
        if (socket != ws) return            // an old socket we already replaced
        setStatus(false, "offline ($why), retrying in ${backoffMs / 1000}s")
        if (wanted) {
            handler.postDelayed({ synchronized(this) { if (wanted) open() } }, backoffMs)
            backoffMs = (backoffMs * 2).coerceAtMost(30_000L)
        }
    }

    private fun setStatus(isConnected: Boolean, text: String) {
        connected = isConnected
        statusText = text
        Log.i(TAG, text)
        listeners.forEach { it.onStatus(isConnected, text) }
    }

    // ---------------------------------------------------------------- send

    fun send(obj: JSONObject): Boolean = ws?.send(obj.toString()) ?: false

    fun notify(app: String, title: String, body: String): Boolean {
        val ok = send(
            JSONObject().put("type", "notify")
                .put("app", app.take(15)).put("title", title.take(23)).put("body", body.take(63))
        )
        if (ok) AppState.logNotification(app, title, body)
        return ok
    }

    fun face(name: String) = send(JSONObject().put("type", "face").put("name", name))
    fun screen(name: String) = send(JSONObject().put("type", "screen").put("name", name))
    fun sound(name: String) = send(JSONObject().put("type", "sound").put("name", name))
    fun ptt(on: Boolean) = send(JSONObject().put("type", "ptt").put("on", on))
    fun think(on: Boolean) = send(JSONObject().put("type", "think").put("on", on))
    fun set(key: String, value: Any, save: Boolean = true) =
        send(JSONObject().put("type", "set").put("key", key).put("value", value).put("save", save))

    /**
     * Stream a spoken reply: 16-bit signed little-endian mono PCM at [rate] Hz.
     * Paced to stay ~200 ms ahead of playback so the device's 0.5 s buffer never overflows.
     */
    fun sayPcm(pcm: ByteArray, rate: Int) {
        speechExecutor.execute {
            val socket = ws ?: return@execute
            socket.send(JSONObject().put("type", "say_start").put("rate", rate).toString())
            val chunk = rate * 2 * SPEECH_CHUNK_MS / 1000
            val start = System.nanoTime()
            var i = 0
            while (i < pcm.size) {
                val end = (i + chunk).coerceAtMost(pcm.size)
                if (!socket.send(pcm.toByteString(i, end - i))) return@execute
                val playedSec = (System.nanoTime() - start) / 1e9
                val aheadSec = (i / 2.0 / rate) - playedSec
                if (aheadSec > 0.2) Thread.sleep(((aheadSec - 0.2) * 1000).toLong())
                i = end
            }
            socket.send(JSONObject().put("type", "say_end").toString())
        }
    }
}
