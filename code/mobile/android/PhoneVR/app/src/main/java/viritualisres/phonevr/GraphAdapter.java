/* (C)2024 */
package viritualisres.phonevr;

import android.view.View;
import com.github.mikephil.charting.charts.LineChart;
import com.github.mikephil.charting.components.YAxis;
import com.github.mikephil.charting.data.Entry;
import com.github.mikephil.charting.data.LineData;
import com.github.mikephil.charting.data.LineDataSet;
import com.github.mikephil.charting.utils.ColorTemplate;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class GraphAdapter {

    private final LineChart chart;
    private List<Long> timestamps = new ArrayList<>();
    private Map<String, List<Float>> lines = new HashMap<>();
    private List<String> keyOrder = new ArrayList<>();
    private List<Integer> color = new ArrayList<>();
    private int len;
    private float max = 10.f;

    public GraphAdapter(int len, View customView) {
        this.len = len;
        this.chart = customView.findViewById(R.id.chart);
    }

    private int checkTimestamp(Long ts) {
        if (!timestamps.contains(ts)) {
            timestamps.add(ts);
            if (timestamps.size() > this.len) {
                timestamps.remove(0);
            }
        }
        return timestamps.indexOf(ts) + 1;
    }

    public void addValue(Long ts, String name, Float value) {
        int position = checkTimestamp(ts);
        if (!lines.containsKey(name)) {
            lines.put(name, new ArrayList<>());
            keyOrder.add(name);
        }
        List<Float> list = lines.get(name);
        list.add(value);
        while (list.size() < position) {
            list.add(0, 0.f);
        }
        if (list.size() > position) {
            list.remove(0);
        }
    }

    public void addValue(Long ts, String name, boolean value) {
        if (value) {
            addValue(ts, name, max);
        } else {
            addValue(ts, name, 0.f);
        }
    }

    public void refresh() {
        // List<LineDataSet> _lines = new ArrayList<>();
        LineData data = new LineData();
        for (int j = 0; j < keyOrder.size(); ++j) {
            String name = keyOrder.get(j);
            List<Float> elems = lines.get(name);
            List<Entry> values = new ArrayList<>();
            for (int i = 0; i < elems.size(); ++i) {
                values.add(new Entry(i, elems.get(i)));
            }
            LineDataSet line = new LineDataSet(values, name);
            if (j > 4) {
                line.setColor(ColorTemplate.LIBERTY_COLORS[j - 5]);
            } else {
                line.setColor(ColorTemplate.JOYFUL_COLORS[j]);
            }
            line.setDrawCircles(false);
            line.setLineWidth(2.f);
            data.addDataSet(line);
        }
        chart.setData(data);

        YAxis yaxis = chart.getAxisLeft();
        chart.getAxisRight().setEnabled(false);
        yaxis.setAxisMaximum(10);
        yaxis.setAxisMinimum(0);

        chart.invalidate();
        // resetViewport();
    }
}
