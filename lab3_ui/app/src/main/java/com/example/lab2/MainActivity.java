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
    public native String readFreqFile();
    public native String readPowerFile();
    public native String readEnergyFile();


    private XYPlot plotEnergy;
    private XYPlot plotPower;
    private XYPlot plotFreq;


    private ArrayList<Integer> freqList = new ArrayList<Integer>();
    private ArrayList<Integer> powerList = new ArrayList<Integer>();
    private ArrayList<Integer> energyList = new ArrayList<Integer>();

    XYSeries seriesEnergy, seriesFreq, seriesPower, series4, series5, series6, series7, series8;
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


    public void startCountDownTimer() {
        countDownTimer = new CountDownTimer(Long.MAX_VALUE, 1000) {
            // This is called after every 0.5 sec interval.
            public void onTick(long millisUntilFinished) {

                String currFreq = readFreqFile();
                currFreq = currFreq.replace("\n", "");
                freqList.add(Integer.valueOf(currFreq));

                String currPower = readPowerFile();
                currPower = currPower.replace("\n", "");
                powerList.add(Integer.valueOf(currPower));

                String currEnergy = readEnergyFile();
                currEnergy = currEnergy.replace("\n", "");
                energyList.add(Integer.valueOf(currEnergy));



                Log.d("lora", "currEnergy: " + String.valueOf(currEnergy));
                Log.d("lora", "currFreq: " + String.valueOf(currFreq));
                Log.d("lora", "currPower: " + String.valueOf(currPower));

                if (countDownTimer != null) {
                    countDownTimer.cancel();
                }

                plotEnergy = (XYPlot) findViewById(R.id.plotEnergy);
                plotPower = (XYPlot) findViewById(R.id.plotPower);
                plotFreq = (XYPlot) findViewById(R.id.plotFreq);

                int color;

                color = Color.MAGENTA;
                seriesFreq = new SimpleXYSeries(freqList, SimpleXYSeries.ArrayFormat.Y_VALS_ONLY, "freq" );
                LineAndPointFormatter seriesFreqFormat = new LineAndPointFormatter(color, color, null, null);
                plotFreq.clear();
                plotFreq.addSeries(seriesFreq, seriesFreqFormat);


                color = Color.YELLOW;
                seriesPower = new SimpleXYSeries(powerList, SimpleXYSeries.ArrayFormat.Y_VALS_ONLY, "power" );
                LineAndPointFormatter seriesPowerFormat = new LineAndPointFormatter(color, color, null, null);
                plotPower.clear();
                plotPower.addSeries(seriesPower, seriesPowerFormat);

                color = Color.CYAN;
                seriesEnergy = new SimpleXYSeries(energyList, SimpleXYSeries.ArrayFormat.Y_VALS_ONLY, "energy" );
                LineAndPointFormatter seriesEnergyFormat = new LineAndPointFormatter(color, color, null, null);
                plotEnergy.clear();
                plotEnergy.addSeries(seriesEnergy, seriesEnergyFormat);





                plotFreq.setRangeBoundaries(0.0, Collections.max(freqList) * 2, BoundaryMode.FIXED);
                PanZoom.attach(plotFreq);
                plotFreq.redraw();
                plotFreq.getGraph().setPaddingLeft(20);

                plotPower.setRangeBoundaries(0.0, Collections.max(powerList) * 2, BoundaryMode.FIXED);
                PanZoom.attach(plotPower);
                plotPower.redraw();
                plotPower.getGraph().setPaddingLeft(20);

                plotEnergy.setRangeBoundaries(0.0, Collections.max(energyList) * 2, BoundaryMode.FIXED);
                PanZoom.attach(plotEnergy);
                plotEnergy.redraw();
                plotEnergy.getGraph().setPaddingLeft(20);




            }

            // Once the job finishes, restart the job.
            public void onFinish() {
                start();
            }
        }.start();
    }

    // On the app initialization.
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        startCountDownTimer();


    }
}