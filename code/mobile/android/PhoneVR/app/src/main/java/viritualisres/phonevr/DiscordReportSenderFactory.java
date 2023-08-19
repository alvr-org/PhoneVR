/* (C)2023 */
package viritualisres.phonevr;

import android.content.Context;
import org.acra.config.CoreConfiguration;
import org.acra.sender.ReportSender;
import org.acra.sender.ReportSenderFactory;
import org.jetbrains.annotations.NotNull;

public class DiscordReportSenderFactory implements ReportSenderFactory {

    @NotNull
    @Override
    public ReportSender create(@NotNull Context context, @NotNull CoreConfiguration config) {
        return new DiscordReportSender();
    }

    @Override
    public boolean enabled(@NotNull CoreConfiguration coreConfig) {
        return true;
    }
}
