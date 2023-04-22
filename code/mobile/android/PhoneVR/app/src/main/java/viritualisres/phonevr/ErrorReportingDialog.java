package viritualisres.phonevr;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.graphics.Color;
import android.os.Bundle;
import android.text.Html;
import android.text.method.LinkMovementMethod;
import android.util.Log;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.acra.data.CrashReportData;
import org.acra.dialog.CrashReportDialogHelper;

import java.net.URLEncoder;
import java.util.Objects;

public class ErrorReportingDialog extends Activity {

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        //android.os.Debug.waitForDebugger();

        CrashReportDialogHelper helper = new CrashReportDialogHelper(this, getIntent());
        try {
            getWindow().getDecorView().setSystemUiVisibility( View.SYSTEM_UI_FLAG_LAYOUT_STABLE | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
            getWindow().setStatusBarColor(Color.TRANSPARENT);

            CrashReportData reportData = helper.getReportData();

            String ExceptionMsg = Objects.requireNonNull(reportData.toMap().get("STACK_TRACE")).toString();
            ExceptionMsg = ExceptionMsg.substring(0, ExceptionMsg.indexOf("\n"));

            AlertDialog builder = new AlertDialog.Builder(this)
                .setTitle("PVR Crash Handler")
                .setIcon(R.mipmap.ic_launcher)
                .setMessage(Html.fromHtml("Unfortunately, PhoneVR has been crashed recently. Developers will be notified.<br/><br/>" +
                        "<b>Exception Message:</b><p style=\"color:red\">" + ExceptionMsg +
                        "</p><br/>You can help us by making an issue in Github's " +
                        "<a href=\"https://github.com/PhoneVR-Developers/PhoneVR/issues/new?title=Phone%20VR%20crash%20Issue...&amp;body="+
                        URLEncoder.encode("Hi, PVR Devs, I am facing a crash with the following Error:\n\n ```ml\n" + ExceptionMsg + "\n```" +
                        "\n\nYou are doing a good job. Hope this helps ;)" , "UTF-8") +
                        "\">Issue Tracker</a>" +
                        " or by talking to us directly on " +
                        "<a href=\"https://discord.gg/pNYtBNk\">Discord.</a>"))
                .setPositiveButton("OK", new DialogInterface.OnClickListener() {
                    public void onClick(DialogInterface dialog, int id) {
                        ErrorReportingDialog.this.finish();
                    }
                })
                .create();
            builder.setCanceledOnTouchOutside(false);
            builder.show();

            ((TextView)builder.findViewById(android.R.id.message)).setMovementMethod(LinkMovementMethod.getInstance());

        } catch (Exception e) {
            Log.d("PVR-Java", "ErrorReportingDialog.OnCreate caught Exception : " + e.getMessage());
            e.printStackTrace();
            finish();
        }
        helper.sendCrash("","");
    }
}
