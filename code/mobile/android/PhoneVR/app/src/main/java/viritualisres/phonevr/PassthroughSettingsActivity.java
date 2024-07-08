/* (C)2024 */
package viritualisres.phonevr;

import android.annotation.SuppressLint;
import android.content.SharedPreferences;
import android.hardware.SensorManager;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import androidx.annotation.NonNull;
import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AppCompatActivity;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceGroupAdapter;
import androidx.preference.PreferenceManager;
import androidx.preference.PreferenceScreen;
import androidx.preference.PreferenceViewHolder;
import androidx.recyclerview.widget.RecyclerView;

public class PassthroughSettingsActivity extends AppCompatActivity {

    private static SharedPreferences sharedPref;
    private static SensorManager sensorManager;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        sharedPref = PreferenceManager.getDefaultSharedPreferences(this);
        sensorManager = (SensorManager) getSystemService(SENSOR_SERVICE);

        setContentView(R.layout.activity_passthrough_settings);
        if (savedInstanceState == null) {
            getSupportFragmentManager()
                    .beginTransaction()
                    .replace(R.id.settings, new SettingsFragment())
                    .commit();
        }
        ActionBar actionBar = getSupportActionBar();
        if (actionBar != null) {
            actionBar.setDisplayHomeAsUpEnabled(true);
        }
    }

    public static class SettingsFragment extends PreferenceFragmentCompat {
        private Passthrough passthrough;
        private GraphAdapter graphAdapter;

        public SettingsFragment() {
            super();
        }

        @Override
        public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
            setPreferencesFromResource(R.xml.passthrough_settings, rootKey);
        }

        @NonNull
        @SuppressLint("RestrictedApi")
        @Override
        protected RecyclerView.Adapter<PreferenceViewHolder> onCreateAdapter(
                PreferenceScreen pref) {
            return new PreferenceGroupAdapter(pref) {
                @NonNull
                public PreferenceViewHolder onCreateViewHolder(
                        @NonNull ViewGroup parent, int viewType) {
                    PreferenceViewHolder holder = super.onCreateViewHolder(parent, viewType);
                    View customLayout = holder.itemView;
                    if (customLayout.getId() == R.id.chartLayout) {
                        graphAdapter = new GraphAdapter(100, customLayout);
                        passthrough = new Passthrough(sharedPref, sensorManager, 640, 480);
                        passthrough.onResume(graphAdapter);
                    }
                    return holder;
                }
            };
        }
    }
}
