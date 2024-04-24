/* (C)2024 */
package viritualisres.phonevr;

import android.annotation.SuppressLint;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import androidx.annotation.NonNull;
import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AppCompatActivity;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceGroupAdapter;
import androidx.preference.PreferenceScreen;
import androidx.preference.PreferenceViewHolder;
import androidx.recyclerview.widget.RecyclerView;
import ir.mahdiparastesh.hellocharts.model.Axis;
import ir.mahdiparastesh.hellocharts.model.Line;
import ir.mahdiparastesh.hellocharts.model.LineChartData;
import ir.mahdiparastesh.hellocharts.model.PointValue;
import ir.mahdiparastesh.hellocharts.model.Viewport;
import ir.mahdiparastesh.hellocharts.util.ChartUtils;
import ir.mahdiparastesh.hellocharts.view.LineChartView;
import java.util.ArrayList;
import java.util.List;

public class PassthroughSettingsActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
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
        private final int numberOfLines = 4;
        private final int numberOfPoints = 12;
        private LineChartView chart = null;

        float[][] randomNumbersTab = new float[numberOfLines][numberOfPoints];

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
                        chart = customLayout.findViewById(R.id.chart);
                        generateValues();
                        generateData(chart);
                        resetViewport(chart);
                    }
                    return holder;
                }
            };
        }

        private void generateValues() {
            for (int i = 0; i < numberOfLines; ++i) {
                for (int j = 0; j < numberOfPoints; ++j) {
                    randomNumbersTab[i][j] = (float) Math.random() * 100f;
                }
            }
        }

        private void generateData(LineChartView chart) {
            List<Line> lines = new ArrayList<>();
            for (int i = 0; i < numberOfLines; ++i) {

                List<PointValue> values = new ArrayList<>();
                for (int j = 0; j < numberOfPoints; ++j) {
                    values.add(new PointValue(j, randomNumbersTab[i][j]));
                }

                Line line = new Line(values);
                line.setColor(ChartUtils.COLORS[i]);
                // line.setShape(shape);
                // line.setCubic(isCubic);
                // line.setFilled(isFilled);
                // line.setHasLabels(hasLabels);
                // line.setHasLabelsOnlyForSelected(hasLabelForSelected);
                // line.setHasLines(hasLines);
                // line.setHasPoints(hasPoints);
                // line.setHasGradientToTransparent(hasGradientToTransparent);
                // if (pointsHaveDifferentColor)
                //    line.setPointColor(ChartUtils.COLORS[(i + 1) % ChartUtils.COLORS.length]);
                lines.add(line);
            }

            LineChartData data = new LineChartData(lines);

            Axis axisX = new Axis();
            Axis axisY = new Axis().setHasLines(true);
            axisX.setName("Axis X");
            axisY.setName("Axis Y");

            data.setAxisXBottom(axisX);
            data.setAxisYLeft(axisY);

            data.setBaseValue(Float.NEGATIVE_INFINITY);
            chart.setLineChartData(data);
        }

        private void resetViewport(LineChartView chart) {
            // Reset viewport height range to (0,100)
            final Viewport v = new Viewport(chart.getMaximumViewport());
            v.bottom = 0;
            v.top = 100;
            v.left = 0;
            v.right = numberOfPoints - 1;
            chart.setMaximumViewport(v);
            chart.setCurrentViewport(v);
        }
    }
}
