/* (C)2024 */
package viritualisres.phonevr;

import ir.mahdiparastesh.hellocharts.model.Axis;
import ir.mahdiparastesh.hellocharts.model.Line;
import ir.mahdiparastesh.hellocharts.model.LineChartData;
import ir.mahdiparastesh.hellocharts.model.PointValue;
import ir.mahdiparastesh.hellocharts.model.Viewport;
import ir.mahdiparastesh.hellocharts.util.ChartUtils;
import ir.mahdiparastesh.hellocharts.view.LineChartView;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class GraphAdapter {

    private final LineChartView chart;
    private List<Long> timestamps = new ArrayList<>();
    private Map<String, List<Float>> lines = new HashMap<>();
    private List<String> keyOrder = new ArrayList<>();
    private List<Integer> color = new ArrayList<>();
    private int len;
    private float max = 10.f;

    public GraphAdapter(int len, LineChartView chart) {
        this.len = len;
        this.chart = chart;
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

    private void resetViewport() {
        Viewport v = new Viewport(chart.getMaximumViewport());
        v.bottom = 0;
        v.top = (int) max;
        v.left = 0;
        v.right = this.len - 1;
        chart.setMaximumViewport(v);
        chart.setCurrentViewport(v);
    }

    public void addValue(Long ts, String name, Float value) {
        int position = checkTimestamp(ts);
        if (!lines.containsKey(name)) {
            lines.put(name, new ArrayList<>());
            keyOrder.add(name);
            color.add(ChartUtils.pickColor());
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
        List<Line> _lines = new ArrayList<>();
        for (int j = 0; j < keyOrder.size(); ++j) {
            String name = keyOrder.get(j);
            List<Float> elems = lines.get(name);
            List<PointValue> values = new ArrayList<>();
            for (int i = 0; i < elems.size(); ++i) {
                values.add(new PointValue(i, elems.get(i)));
            }
            Line line = new Line(values);
            line.setColor(color.get(j));
            line.setHasPoints(false);
            _lines.add(line);
        }
        LineChartData data = new LineChartData(_lines);

        Axis axisX = new Axis();
        Axis axisY = new Axis().setHasLines(true);
        axisX.setName("Time");
        axisY.setName("Values");

        data.setAxisXBottom(axisX);
        data.setAxisYLeft(axisY);

        data.setBaseValue(Float.NEGATIVE_INFINITY);
        chart.setLineChartData(data);
        resetViewport();
    }
}
