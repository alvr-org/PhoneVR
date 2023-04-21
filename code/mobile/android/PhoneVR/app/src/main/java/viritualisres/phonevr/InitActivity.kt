package viritualisres.phonevr

import android.content.Intent
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.view.View
import android.widget.TextView

class InitActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_init)

        val tvVersion : TextView = findViewById<TextView>(R.id.head);
        tvVersion.text = "PhoneVR v" + BuildConfig.VERSION_NAME + "_" + BuildConfig.VERSION_CODE;
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