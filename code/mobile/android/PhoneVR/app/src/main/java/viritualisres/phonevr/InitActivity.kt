package viritualisres.phonevr

import android.content.Intent
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.text.Html
import android.text.Html.FROM_HTML_MODE_LEGACY
import android.text.Spanned
import android.text.method.LinkMovementMethod
import android.text.util.Linkify
import android.util.Log
import android.view.View
import android.widget.TextView
import androidx.core.text.HtmlCompat

class InitActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_init)

        val tvVersion : TextView = findViewById<TextView>(R.id.head);
        tvVersion.text = "PhoneVR v" + BuildConfig.VERSION_NAME;

        val result = HtmlCompat.fromHtml("<a href=\"https://github.com/PhoneVR-Developers/PhoneVR#readme\">Readme.md</a>", HtmlCompat.FROM_HTML_MODE_LEGACY);
        val tvBody = findViewById<TextView>(R.id.body);
        Log.d("PVR-JAVA", "res: '${ resources.getString(R.string.phonevr_support_packages) }', html: '${result.toString()}'");

        tvBody.text = String.format(resources.getString(R.string.phonevr_support_packages), result);
        tvBody.movementMethod = LinkMovementMethod.getInstance();
        Linkify.addLinks(tvBody, Linkify.WEB_URLS);
    }

    fun btOnClickALVRStreamer(view: View) {
        val intent = Intent(this, ALVRActivity::class.java)
        startActivity(intent)
    }
    fun btOnClickPVRStreamer(view: View) {
        val intent = Intent(this, MainActivity::class.java)
        startActivity(intent)
    }
}