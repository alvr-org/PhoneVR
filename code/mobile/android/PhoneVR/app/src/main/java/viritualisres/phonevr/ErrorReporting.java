package viritualisres.phonevr;

import android.app.Application;
import android.content.Context;

import org.acra.ACRA;
import org.acra.annotation.AcraCore;
import org.acra.annotation.AcraToast;
import org.acra.config.CoreConfigurationBuilder;
import org.acra.data.StringFormat;

import static org.acra.ReportField.*;

@AcraCore(buildConfigClass = BuildConfig.class,
        reportFormat = StringFormat.JSON,
        reportContent = {
            ANDROID_VERSION,
            APP_VERSION_CODE,
            APP_VERSION_NAME,
            //APPLICATION_LOG,
            AVAILABLE_MEM_SIZE,
            BRAND,
            BUILD,
            BUILD_CONFIG,
            CRASH_CONFIGURATION,
            CUSTOM_DATA,
            DEVICE_FEATURES,
            DEVICE_ID,
            DISPLAY,
            DROPBOX,
            ENVIRONMENT,
            EVENTSLOG,
            FILE_PATH,
            INITIAL_CONFIGURATION,
            INSTALLATION_ID,
            IS_SILENT,
            LOGCAT,
            MEDIA_CODEC_LIST,
            PACKAGE_NAME,
            PHONE_MODEL,
            PRODUCT,
            RADIOLOG,
            REPORT_ID,
            SETTINGS_GLOBAL,
            SETTINGS_SECURE,
            SETTINGS_SYSTEM,
            SHARED_PREFERENCES,
            STACK_TRACE,
            STACK_TRACE_HASH,
            THREAD_DETAILS,
            TOTAL_MEM_SIZE,
            USER_APP_START_DATE,
            USER_COMMENT,
            USER_CRASH_DATE,
            USER_EMAIL },
            reportSenderFactoryClasses = DiscordReportSenderFactory.class)

//@AcraMailSender(mailTo = "phonevr.crash@gmail.com")
@AcraToast(resText = R.string.app_crash_toast)

public class ErrorReporting extends Application {
    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(base);
        CoreConfigurationBuilder builder = new CoreConfigurationBuilder(this)
                .setBuildConfigClass(BuildConfig.class);

        /*SimpleDateFormat sdf = new SimpleDateFormat("dd-MM-yyyy HH:mm:ss", Locale.getDefault());
        String currentDateandTime = sdf.format(new Date());

        builder.getPluginConfigurationBuilder(MailSenderConfigurationBuilder.class)
                .setReportAsFile(true)
                .setSubject("Crash report on " + currentDateandTime)
                .setBody("Hello PVR Team, I faced a new Crash...")
                .setReportFileName("phonevrcrash.staketrace")
                .setEnabled(true);*/

        ACRA.init(this, builder);
    }
}
