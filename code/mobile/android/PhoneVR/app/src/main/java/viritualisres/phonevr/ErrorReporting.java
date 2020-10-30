package viritualisres.phonevr;
import android.app.Application;
import android.content.Context;

import org.acra.*;
import org.acra.annotation.*;
import org.acra.config.CoreConfigurationBuilder;
import org.acra.config.MailSenderConfigurationBuilder;
import org.acra.data.StringFormat;

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

import static org.acra.ReportField.EVENTSLOG;
import static org.acra.ReportField.MEDIA_CODEC_LIST;
import static org.acra.ReportField.RADIOLOG;

@AcraCore(buildConfigClass = BuildConfig.class, reportContent = {EVENTSLOG, RADIOLOG, MEDIA_CODEC_LIST}, reportFormat = StringFormat.JSON)
@AcraMailSender(mailTo = "phonevr.crash@gmail.com")
public class ErrorReporting extends Application {
    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(base);
        CoreConfigurationBuilder builder = new CoreConfigurationBuilder(this)
                .setBuildConfigClass(BuildConfig.class);

        SimpleDateFormat sdf = new SimpleDateFormat("dd MM yyyy_HH:mm:ss", Locale.getDefault());
        String currentDateandTime = sdf.format(new Date());

        builder.getPluginConfigurationBuilder(MailSenderConfigurationBuilder.class)
                .setReportAsFile(true)
                .setSubject("Crash report on " + currentDateandTime);
        // The following line triggers the initialization of ACRA
        ACRA.init(this);
    }
}
