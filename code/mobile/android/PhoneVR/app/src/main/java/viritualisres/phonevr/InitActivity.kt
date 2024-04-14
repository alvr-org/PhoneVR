/* (C)2023 */
package viritualisres.phonevr

import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.text.Html.FROM_HTML_MODE_LEGACY
import android.text.method.LinkMovementMethod
import android.text.util.Linkify
import android.util.Log
import android.view.View
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.text.HtmlCompat

class InitActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_init)

        val tvVersion: TextView = findViewById<TextView>(R.id.head)
        tvVersion.text = "PhoneVR v" + BuildConfig.VERSION_NAME

        val result =
            HtmlCompat.fromHtml(
                "<a href=\"https://github.com/PhoneVR-Developers/PhoneVR#readme\">Readme.md</a>",
                HtmlCompat.FROM_HTML_MODE_LEGACY)
        val tvBody = findViewById<TextView>(R.id.body)
        Log.d(
            "PVR-JAVA",
            "res: '${ resources.getString(R.string.phonevr_support_packages) }', html: '${result.toString()}'")

        tvBody.text = String.format(resources.getString(R.string.phonevr_support_packages), result)
        tvBody.movementMethod = LinkMovementMethod.getInstance()
        Linkify.addLinks(tvBody, Linkify.WEB_URLS)
    }

    fun btOnClickALVRStreamer(view: View) {
        val intent = Intent(this, ALVRActivity::class.java)
        startActivity(intent)
    }

    fun btOnClickPVRStreamer(view: View) {
        // Check if SYSTEM ABI supports other than x86_64 ABI, since libGVR has no support for
        // x86_64
        if (Build.SUPPORTED_ABIS.contains("x86") ||
            Build.SUPPORTED_ABIS.contains("arm64-v8a") ||
            Build.SUPPORTED_ABIS.contains("armeabi-v7a")) {
            val intent = Intent(this, MainActivity::class.java)
            startActivity(intent)
        } else {
            Toast.makeText(
                    this,
                    "Your device ONLY supports x86_64 ABI, which is not supported by GoogleVR",
                    Toast.LENGTH_LONG)
                .show()
        }
    }
}
