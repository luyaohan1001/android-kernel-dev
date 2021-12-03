package com.example.lab2;

import androidx.appcompat.app.AppCompatActivity;

import android.graphics.Color;
import android.os.CountDownTimer;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.TableLayout;
import android.widget.TableRow;
import android.widget.TextView;

import android.os.Bundle;

import android.app.Activity;
import android.graphics.*;
import android.os.Bundle;

import com.androidplot.util.PixelUtils;
import com.androidplot.xy.SimpleXYSeries;
import com.androidplot.xy.XYSeries;
import com.androidplot.xy.*;

import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.text.FieldPosition;
import java.text.Format;
import java.text.ParsePosition;
import java.util.*;

import java.util.HashMap;
import java.util.Map;





public class MainActivity extends AppCompatActivity {

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("calc");
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    public native int Jniint();
    public native String readUtilsFilesContent(String file);
    public native String[] readUtilsFilesName(int numOfUtilFiles);
    public native void startMonitoring();
    public native void stopMonitoring();
    public native void startTaskReserve (int threadID, int C, int T, int cpuID);
    public native void cancelTaskReserve (int threadID);
    public native int findNumUtilFiles();
    public native int add(int a, int b);

    private XYPlot plot;

    private Hashtable<String, ArrayList> utils_history = new Hashtable<String, ArrayList>();
    private HashMap<String, String> utilsMap = new HashMap<String, String>();

    XYSeries series1, series2, series3, series4, series5, series6, series7, series8;
    CountDownTimer countDownTimer;
    String[] utils_files_name_arr;

    public static boolean isNumeric(String str) {
        try {
            Double.parseDouble(str);
            return true;
        } catch(NumberFormatException e){
            return false;
        }
    }
    private float getAvgUtil(String[][] periodtime_util_aoa) {
        float total_utils = 0;
        // float total_time = 1;
        int length = periodtime_util_aoa.length;
        // total time is the first element on the last row
        float total_time;
        if (Float.valueOf(periodtime_util_aoa[length-1][0]) != Float.valueOf(periodtime_util_aoa[0][0])) {
            total_time = Float.valueOf(periodtime_util_aoa[length-1][0]) - Float.valueOf(periodtime_util_aoa[0][0]);
        } else {
            total_time = Float.valueOf(periodtime_util_aoa[0][0]);
        }

        for (int i = 0; i < periodtime_util_aoa.length; i++) {
            Log.d("HAN","START---");
            Log.d("HAN",periodtime_util_aoa[i][1]);
            if (!(periodtime_util_aoa[i][1]).equals("")) {
                total_utils += Float.valueOf(periodtime_util_aoa[i][1]);
                Log.d("HAN",String.valueOf(Float.valueOf(periodtime_util_aoa[i][1])));
            }
        }

        return (total_utils);
    }




    public void startCountDownTimer() {
        countDownTimer = new CountDownTimer(Long.MAX_VALUE, 500) {
            // This is called after every 0.5 sec interval.
            public void onTick(long millisUntilFinished) {

                //int num_of_util_files = utils_files_name_arr.length;
                int num_of_util_files = findNumUtilFiles();
                utils_files_name_arr = readUtilsFilesName(num_of_util_files);
                Log.d("BELLPEPPER", "We got a file length " +  String.valueOf(num_of_util_files)) ;
                // Initialize the utils map
                for (int i = 0; i < num_of_util_files; i ++) {
                    Log.d("BELLPEPPER", "*file_name is: " +  String.valueOf(utils_files_name_arr[i])) ;
                    String content_single_file = readUtilsFilesContent(utils_files_name_arr[i]);
                    String file_name = utils_files_name_arr[i];

                    if (content_single_file.equals('\0')) {
                        break;
                    }
                    ArrayList<String> utils_list = new ArrayList<String>(Arrays.asList(content_single_file.split("\n")));
                    if (utils_history.get(file_name) == null) {
                        utils_history.put(file_name, utils_list);
                    } else {
                        Log.d("APPEND","appended!");
                        utils_history.get(file_name).addAll(utils_list);
                    }
                    // utils_history.put(file_name, utils_list);
                    }

                final TableLayout tableLayout_threadTable = (TableLayout) findViewById(R.id.tableLayout_threadTable);
                final TableLayout tableLayout_selectList = (TableLayout) findViewById(R.id.tableLayout_selectList);
                tableLayout_threadTable.removeAllViewsInLayout();
                tableLayout_selectList.removeAllViewsInLayout();


                TableRow titleRow = new TableRow(getBaseContext());
                TextView thread_title = new TextView(getBaseContext());
                TextView util_title = new TextView(getBaseContext());
                thread_title.setText("Thread");
                thread_title.setText("Averager Util");
                titleRow.addView(thread_title);
                titleRow.addView(util_title);
                tableLayout_threadTable.addView(titleRow);


                // String[] utils_files_content_arr = readUtilsFilesContent();
                for (Map.Entry<String, ArrayList> thread : utils_history.entrySet()) {

                    // Printing all elements of a Map
                    // Create number of tableRows according to the file name

                    TableRow currRow = new TableRow(getBaseContext());
                    String threadName = thread.getKey();

                    float average = 0;
                    float sum = 0;
                    float time_start = 0;
                    float time_end = 0;
                    float time_diff = 0;
                    for (int i = 0; i < thread.getValue().size(); i++) {
                        String row = (String)thread.getValue().get(i);
                        String[] row_splited = row.split("  ");
                        if (row_splited[0].isEmpty()) break;
                        if (i == 0) time_start = Float.valueOf(row_splited[0]);
                        time_end = Float.valueOf(row_splited[0]);

                        time_diff = time_end - time_start;
                        sum += Float.valueOf(row_splited[1]);
                    }
                    float threadUtil = sum / time_diff;
                    TextView thread_item = new TextView(getBaseContext());
                    TextView util_item = new TextView(getBaseContext());
                    thread_item.setText(String.valueOf(threadName));
                    util_item.setText(String.valueOf(threadUtil));

                    currRow.addView(thread_item);
                    currRow.addView(util_item);
                    tableLayout_threadTable.addView(currRow);

                    // Add thread item to the select list.
                    TableRow selectRow = new TableRow(getBaseContext());


                    Button select_item = new Button(getBaseContext());
                    select_item.setText(String.valueOf(threadName));
                    selectRow.addView(select_item);
                    tableLayout_selectList.addView(selectRow);

                    select_item.setOnClickListener(new View.OnClickListener() {
                        @Override
                        public void onClick(View view) {
                            final TextView editTextView_targetTheadId = (TextView) findViewById(R.id.editTextNumber_targetThreadId);
                            editTextView_targetTheadId.setText(String.valueOf(threadName));

                        }
                    });
                }
            }
            public void onFinish() {
                start();
            }
        }.start();
    }


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        try {
            Runtime.getRuntime().exec("su");
        } catch (IOException e) {
            e.printStackTrace();
        }

        // End of Android Plot Graph.


        // Find the "setC" TextView
        final TextView textView_C = (TextView)findViewById(R.id.textView_C);
        final TextView textView_T = (TextView)findViewById(R.id.textView_T);
        final TextView textView_cpuID = (TextView)findViewById(R.id.textView_cpuID);
        final TextView textView_targetThreadId = (TextView)findViewById(R.id.textView_targetThreadId);

        textView_C.setText("Enter completion time ");
        textView_T.setText("Enter period ");
        // textView_cpuID.setText("Enter CPU ID");
        textView_cpuID.setText("Enter CPU Id");
        textView_targetThreadId.setText("Enter target thread ID");
        // Calculator
        final TextView editTextView_calcIn1 = (TextView)findViewById(R.id.editTextNumber_C);
        final TextView editTextView_calcIn2 = (TextView)findViewById(R.id.editTextNumber_T);
        final TextView editTextView_calcResult = (TextView)findViewById(R.id.editTextNumber_cpuID);


        final TextView editTextView_C = (TextView)findViewById(R.id.editTextNumber_C);
        final TextView editTextView_T = (TextView)findViewById(R.id.editTextNumber_T);
        final TextView editTextView_cpuID = (TextView)findViewById(R.id.editTextNumber_cpuID);
        final TextView editTextView_targetTheadId = (TextView) findViewById(R.id.editTextNumber_targetThreadId);



        // Create number of tableRows according to the file name
        final TableLayout tableLayout_threadTable = (TableLayout) findViewById(R.id.tableLayout_threadTable);
        final TableLayout tableLayout_selectList = (TableLayout) findViewById(R.id.tableLayout_selectList);


        editTextView_calcIn1.setText("250");
        editTextView_calcIn2.setText("500");

        // Find the "setC" button
        final Button button_setTask= (Button)findViewById(R.id.button_setTask);
        button_setTask.setText("Reserve");
        button_setTask.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick (View view){
                int calcIn1 = Integer.valueOf(editTextView_calcIn1.getText().toString());
                int calcIn2 = Integer.valueOf(editTextView_calcIn2.getText().toString());
                //textView_setC.setText("Hello World!");
                // textView_C.setText(String.valueOf(Jniint()));
                // editTextView_calcResult.setText(String.valueOf(add(calcIn1, calcIn2)));
                // String[] utils_files_name_arr = readUtilsFilesName();
                // String[] utils_files_content_arr = readUtilsFilesContent();
                // editTextView_calcResult.setText(readUtilsFilesContent()[0]);
                String threadID = String.valueOf(editTextView_targetTheadId.getText());
                String C = String.valueOf(editTextView_C.getText());
                String T = String.valueOf(editTextView_T.getText());
                String cpuID = String.valueOf(editTextView_cpuID.getText());

                if ( !C.isEmpty() && !T.isEmpty() && !cpuID.isEmpty() && !threadID.isEmpty()) {
                    startTaskReserve(Integer.valueOf(threadID), Integer.valueOf(C), Integer.valueOf(T), Integer.valueOf(cpuID));
                    try {
                        Process process = Runtime.getRuntime().exec("su");
                        process.waitFor();
                    } catch (IOException e) {
                        e.printStackTrace();
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                    try {
                        // Executes the command.
                        Process process = Runtime.getRuntime().exec("/data/rtes/apps/reserve/reserve set 6565 450 500 0");
                    } catch (IOException e) {
                        throw new RuntimeException(e);
                    }
                }
            }
        });


        // Find the "cancel" button
        final Button button_cancelTask = (Button)findViewById(R.id.button_cancelTask);
        button_cancelTask.setText("Cancel");
        button_cancelTask.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick (View view){
                int calcIn1 = Integer.valueOf(editTextView_calcIn1.getText().toString());
                int calcIn2 = Integer.valueOf(editTextView_calcIn2.getText().toString());
                //textView_setC.setText("Hello World!");
                // textView_C.setText(String.valueOf(Jniint()));
                // editTextView_calcResult.setText(String.valueOf(add(calcIn1, calcIn2)));
                // String[] utils_files_name_arr = readUtilsFilesName();
                // String[] utils_files_content_arr = readUtilsFilesContent();
                // editTextView_calcResult.setText(readUtilsFilesContent()[0]);

                String threadID = String.valueOf(editTextView_targetTheadId.getText());
                if (!threadID.isEmpty()) {
                    cancelTaskReserve(Integer.valueOf(threadID));
                }
                // Make reserve
            }
        });




        final Button button_startMon = (Button) findViewById(R.id.button_startMon);
        button_startMon.setText("Start Mon");
        button_startMon.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick (View view){
                Log.d("HAN","start MON DETECTED!");
                // set enabled to 1
                if (series1 != null)  plot.removeSeries(series1);
                if (series2 != null)  plot.removeSeries(series2);
                if (series3 != null)  plot.removeSeries(series3);
                if (series4 != null)  plot.removeSeries(series4);
                if (series5 != null)  plot.removeSeries(series5);
                if (series6 != null)  plot.removeSeries(series6);
                if (series7 != null)  plot.removeSeries(series7);
                if (series8 != null)  plot.removeSeries(series8);
                series1 = null;
                series2 = null;
                series3 = null;
                series4 = null;
                series5 = null;
                series6 = null;
                series7 = null;
                series8 = null;

                startMonitoring();
                startCountDownTimer();
            }
        });


        final Button button_stopMon = (Button) findViewById(R.id.button_stopMon);
        button_stopMon.setText("Stop Mon");
        button_stopMon.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick (View view) {
                // set enabled to 0
                stopMonitoring();
                if (countDownTimer != null) {
                    countDownTimer.cancel();
                }


                plot = (XYPlot) findViewById(R.id.plot);
                // create a couple arrays of y-values to plot:
                if (series1 != null)  plot.removeSeries(series1);
                if (series2 != null)  plot.removeSeries(series2);
                if (series3 != null)  plot.removeSeries(series3);
                if (series4 != null)  plot.removeSeries(series4);
                if (series5 != null)  plot.removeSeries(series5);
                if (series6 != null)  plot.removeSeries(series6);
                if (series7 != null)  plot.removeSeries(series7);
                if (series8 != null)  plot.removeSeries(series8);
                if(utils_files_name_arr != null) {
                    Log.d("HAN","here");
                    // initialize our XYPlot reference:
                    plot = (XYPlot) findViewById(R.id.plot);



                    if (!utils_history.isEmpty()) {
                        int count = 0;
                        for (String thread : utils_history.keySet()) {
                            // Gets the arraylist of data

                            // create a couple arrays of y-values to plot:
                            ArrayList<Integer> xs = new ArrayList<>();
                            ArrayList<Float> numbers = new ArrayList<>();
                            for (int i = 0; i < utils_history.get(thread).size(); i++) {
                                String row = (String) utils_history.get(thread).get(i);
                                String[] row_splited = row.split("  ");
                                if (row_splited.length < 2) break;
                                Log.d("HUH", "Row thread is: " + thread);
                                numbers.add(Float.valueOf(row_splited[1]));
                            }

                            for (int i = 1; i <= numbers.size(); i++) {
                                xs.add(i);
                            }

                            int color = Color.GREEN;
                            if (count == 0)color = Color.GREEN;
                            else if (count == 1)color = Color.YELLOW;
                            else if (count == 2)color = Color.BLUE;
                            else if (count == 3)color = Color.RED;
                            else if (count == 4)color = Color.WHITE;
                            else if (count == 5)color = Color.CYAN;
                            else if (count == 6)color = Color.MAGENTA;
                            else if (count == 7)color = Color.GRAY;
                            count ++;

                            if (series1 == null) {
                                series1 = new SimpleXYSeries(numbers, SimpleXYSeries.ArrayFormat.Y_VALS_ONLY, "thread"+String.valueOf(thread));
                                LineAndPointFormatter series1Format = new LineAndPointFormatter(color, color, null,null);
                                plot.addSeries(series1, series1Format);
                            }
                            else if (series2 == null) {
                                series2 = new SimpleXYSeries(numbers, SimpleXYSeries.ArrayFormat.Y_VALS_ONLY, "thread"+String.valueOf(thread));
                                LineAndPointFormatter series2Format = new LineAndPointFormatter(color, color, null,null);
                                plot.addSeries(series2, series2Format);
                            }
                            else if (series3 == null) {
                                series3 = new SimpleXYSeries(numbers, SimpleXYSeries.ArrayFormat.Y_VALS_ONLY, "thread"+String.valueOf(thread));
                                LineAndPointFormatter series3Format = new LineAndPointFormatter(color, color, null,null);
                                plot.addSeries(series3, series3Format);
                            }
                            else if (series4 == null) {
                                series4 = new SimpleXYSeries(numbers, SimpleXYSeries.ArrayFormat.Y_VALS_ONLY, "thread"+String.valueOf(thread));
                                LineAndPointFormatter series4Format = new LineAndPointFormatter(color, color, null,null);
                                plot.addSeries(series4, series4Format);
                            }
                            else if (series5 == null) {
                                series5 = new SimpleXYSeries(numbers, SimpleXYSeries.ArrayFormat.Y_VALS_ONLY, "thread"+String.valueOf(thread));
                                LineAndPointFormatter series5Format = new LineAndPointFormatter(color, color, null,null);
                                plot.addSeries(series5, series5Format);
                            }
                            else if (series6 == null) {
                                series6 = new SimpleXYSeries(numbers, SimpleXYSeries.ArrayFormat.Y_VALS_ONLY, "thread"+String.valueOf(thread));
                                LineAndPointFormatter series6Format = new LineAndPointFormatter(color, color, null,null);
                                plot.addSeries(series6, series6Format);

                            }
                            else if (series7 == null) {
                                series7 = new SimpleXYSeries(numbers, SimpleXYSeries.ArrayFormat.Y_VALS_ONLY, "thread"+String.valueOf(thread));
                                LineAndPointFormatter series7Format = new LineAndPointFormatter(color, color, null,null);
                                plot.addSeries(series7, series7Format);
                            }
                            else if (series8 == null) {
                                series8 = new SimpleXYSeries(numbers, SimpleXYSeries.ArrayFormat.Y_VALS_ONLY, "thread"+String.valueOf(thread));
                                LineAndPointFormatter series8Format = new LineAndPointFormatter(color, color, null,null);
                                plot.addSeries(series8, series8Format);
                            }
                        }



                    }
                    plot.setRangeBoundaries(0.0, 0.005, BoundaryMode.FIXED );
                    PanZoom.attach(plot);
                    plot.redraw();

                }
            }
        });
    }



}