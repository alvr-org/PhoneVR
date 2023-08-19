/* (C)2023 */
package viritualisres.phonevr;

import android.content.Context;
import android.os.AsyncTask;
import android.util.Log;
import java.io.BufferedOutputStream;
import java.io.BufferedWriter;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.Objects;
import java.util.UUID;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import org.acra.data.CrashReportData;
import org.acra.sender.ReportSender;
import org.acra.util.BundleWrapper;
import org.jetbrains.annotations.NotNull;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

public class DiscordReportSender implements ReportSender {

    @Override
    public void send(
            @NotNull Context context,
            @NotNull CrashReportData report,
            @NotNull BundleWrapper extras) {
        try {
            // Log.d("ACRA-PVR", "Waiting for debugger to Attach !");
            // android.os.Debug.waitForDebugger();
            // Log.d("ACRA-PVR", "Debugger Attached XD");
            PostDiscord send = new PostDiscord(report);
            send.doInBackground();
            Log.d("ACRA-PVR", "Called Send3");

        } catch (JSONException e) {
            Log.d("ACRA-PVR", "Caught Exception : " + e.getMessage());
            e.printStackTrace();
        }
    }

    public static class PostDiscord extends AsyncTask<String, String, Void> {

        String FileData, ExceptionMsg;
        CrashReportData reportsData;

        public PostDiscord(CrashReportData report) throws JSONException {

            FileData = report.toJSON().toString();
            ExceptionMsg = Objects.requireNonNull(report.toMap().get("STACK_TRACE")).toString();
            ExceptionMsg = ExceptionMsg.substring(0, ExceptionMsg.indexOf("\n"));
            reportsData = report;
        }

        @Override
        protected void onPreExecute() {
            super.onPreExecute();
        }

        @Override
        protected Void doInBackground(String... params) {
            try {

                // String urlString =
                // "https://discord.com/api/webhooks/772915848775991310/teIX_7vPM30wrx5bjs1iFsHPBIL-twth6iPH1cd5VW5W2goxr50SmPT3l3O4X7iB9bSG"; // URL to call
                String urlString =
                        "https://discord.com/api/webhooks/1095611501554958356/oIAVJopxxG0i_MAAjwOS0FwV4_ycIG3709gs4zIlCpyLpCNcbBCGiB1mo7B1qCB_uk7z"; // URL to call
                URL url = new URL(urlString);
                String boundary = UUID.randomUUID().toString();
                HttpURLConnection urlConnection = (HttpURLConnection) url.openConnection();

                urlConnection.setRequestMethod("POST");
                urlConnection.setRequestProperty(
                        "Content-Type", "multipart/form-data; boundary=" + boundary);
                urlConnection.setDoOutput(true);

                OutputStream out = new BufferedOutputStream(urlConnection.getOutputStream());
                BufferedWriter writer =
                        new BufferedWriter(new OutputStreamWriter(out, StandardCharsets.UTF_8));

                int firstLetter =
                        Character.codePointAt(
                                        ((JSONObject) reportsData.get("INITIAL_CONFIGURATION"))
                                                .get("locale")
                                                .toString(),
                                        3)
                                - 0x41
                                + 0x1F1E6;
                int secondLetter =
                        Character.codePointAt(
                                        ((JSONObject) reportsData.get("INITIAL_CONFIGURATION"))
                                                .get("locale")
                                                .toString(),
                                        4)
                                - 0x41
                                + 0x1F1E6;

                String gitFileUrl =
                        "https://github.com/PhoneVR-Developers/PhoneVR/blob/master/code/mobile/android/PhoneVR/app/src/main/java/viritualisres/phonevr/";

                Pattern p =
                        Pattern.compile(
                                "(viritualisres\\.phonevr[^{}():\\n\\t]*?\\(.*?:[0-9]*?\\).*?)\\n");
                Matcher m = p.matcher(reportsData.get("STACK_TRACE").toString());
                StringBuilder pvrRelatedFiles = new StringBuilder();

                while (m.find()) {
                    pvrRelatedFiles.append(
                            " --> "
                                    + m.group(0)
                                            .replaceAll("\n|\t", "")
                                            .replaceAll(
                                                    "\\((.*?):([0-9]*?)\\).*?",
                                                    "([$1:$2](" + gitFileUrl + "$1#L$2))")
                                    + "\n");
                }
                JSONObject JsonPayload = new JSONObject();

                JsonPayload.put(
                        "embeds",
                        new JSONArray()
                                .put(
                                        new JSONObject()
                                                .put(
                                                        "author",
                                                        new JSONObject()
                                                                .put("name", "Github CrossRef")
                                                                .put(
                                                                        "url",
                                                                        "https://github.com/ShootingKing-AM")
                                                                .put(
                                                                        "icon_url",
                                                                        "https://github.githubassets.com/images/modules/logos_page/GitHub-Mark.png"))
                                                .put(
                                                        "footer",
                                                        new JSONObject()
                                                                .put(
                                                                        "icon_url",
                                                                        "https://avatars0.githubusercontent.com/u/4137788?s=100")
                                                                .put("text", "ShootinKing-AM"))
                                                .put("color", 16760388)
                                                .put("title", "Stack trace")
                                                .put("description", pvrRelatedFiles)));

                JsonPayload.put(
                        "content",
                        "An `"
                                + ((JSONObject) reportsData.get("BUILD")).getString("MANUFACTURER")
                                + " "
                                + ((JSONObject) reportsData.get("BUILD")).get("MODEL")
                                + " Android v"
                                + reportsData.get("ANDROID_VERSION")
                                + "("
                                + ((JSONObject) reportsData.get("INITIAL_CONFIGURATION"))
                                        .get("locale")
                                + ") `"
                                + (new String(Character.toChars(firstLetter))
                                        + new String(Character.toChars(secondLetter)))
                                + "  crashed on ```css\n"
                                + "Time:           "
                                + reportsData.get("USER_CRASH_DATE")
                                + "\n"
                                + "App Version:    "
                                + ((JSONObject) reportsData.get("BUILD_CONFIG")).get("VERSION_NAME")
                                + "_"
                                + ((JSONObject) reportsData.get("BUILD_CONFIG")).get("VERSION_CODE")
                                + "```*With:*\n```ml\n"
                                + ExceptionMsg
                                + "```\n");

                writer.write("--" + boundary + "\r\n");
                writer.write(
                        "Content-Disposition: form-data; name=\"file\";"
                                + " filename=\"pvrcrash.stacktrace\" \r\n\r\n");
                writer.write(FileData + "\r\n");
                writer.write("--" + boundary + "\r\n");

                writer.write("Content-Disposition: form-data; name=\"payload_json\"\r\n\r\n");
                writer.write(JsonPayload.toString() + "\r\n");
                writer.write("--" + boundary + "--\r\n");

                writer.flush();
                writer.close();
                out.close();

                urlConnection.connect();
                Log.d(
                        "ACRA-PVR",
                        "Connection Resp Code : "
                                + urlConnection.getResponseCode()
                                + "; Resp Msg : "
                                + urlConnection.getResponseMessage());
                urlConnection.getInputStream().close();
                urlConnection.disconnect();

            } catch (Exception e) {
                Log.d("ACRA-PVR", "Exception Caught : " + e.getMessage());
                e.printStackTrace();
            }
            return null;
        }
    }
}
