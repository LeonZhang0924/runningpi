package com.hackathon.rpi;

import java.io.IOException;
import java.net.Socket;
import java.net.UnknownHostException;

import android.app.Activity;
import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.AsyncTask;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.widget.TextView;
import android.widget.Toast;

import com.camera.simplemjpeg.MjpegInputStream;
import com.camera.simplemjpeg.MjpegView;
import com.google.android.gms.maps.CameraUpdateFactory;
import com.google.android.gms.maps.GoogleMap;
import com.google.android.gms.maps.MapFragment;
import com.google.android.gms.maps.model.LatLng;
import com.google.android.gms.maps.model.MarkerOptions;

public class Main extends Activity implements SensorEventListener {
	/* sensor data */
    SensorManager m_sensorManager;
    float []m_lastMagFields;
    float []m_lastAccels;
    private float[] m_rotationMatrix = new float[16];
    private float[] m_orientation = new float[4];
 
    /* fix random noise by averaging tilt values */
    final static int AVERAGE_BUFFER = 30;
    float []m_prevPitch = new float[AVERAGE_BUFFER];
    float m_lastPitch = 0.f;
    float m_lastYaw = 0.f;
    /* current index int m_prevEasts */
    int m_pitchIndex = 0;
 
    float []m_prevRoll = new float[AVERAGE_BUFFER];
    float m_lastRoll = 0.f;
    /* current index into m_prevTilts */
    int m_rollIndex = 0;
    
    private static Context mContext;
    private boolean mConnected = false;
    private TcpClient mTcpClient;
    private long mTime;
    private static Toast mCurrentToast = null;
    private WebViewFrag mStream;
    private boolean mStreaming = false;
    private GoogleMap mMap;
    private MjpegView mv = null;
    
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
        
        mContext = this;
        
        /*mStream = (WebViewFrag) getFragmentManager().findFragmentById(R.id.stream);*/
        mv = (MjpegView) findViewById(R.id.mv);
        m_sensorManager = (SensorManager) getSystemService(Context.SENSOR_SERVICE);
        registerListeners();
        setUpMapIfNeeded();
    }
    
    @Override
	public boolean onCreateOptionsMenu(Menu menu) {
		MenuInflater inflater = getMenuInflater();
		inflater.inflate(R.menu.main, menu);
		MenuItem item = menu.findItem(R.id.menuitem_connect);
		if (mConnected) {
			item.setIcon(android.R.drawable.ic_menu_close_clear_cancel);
		} else {
			item.setIcon(android.R.drawable.ic_menu_call);
		}
		return true;
	}
 
    @Override
	public boolean onOptionsItemSelected(MenuItem item) {
		switch (item.getItemId()) {
		case R.id.menuitem_connect:
			if (!mConnected) {
				showToast(getResources().getString(R.string.menu_connecting));
				
				// Start server connection
				new ConnectTask().execute();
				mConnected = true;
				item.setIcon(android.R.drawable.ic_menu_close_clear_cancel);
			} else {
				showToast(getResources().getString(R.string.menu_disconnecting));
				
				// Disconnect from server
				if (mTcpClient != null) {
					mTcpClient.stopClient();
	                mTcpClient = null;
				}
                
				mConnected = false;
				item.setIcon(android.R.drawable.ic_menu_call);
			}
			return true;
		case R.id.menuitem_stream:
			if (!mStreaming) {
				item.setIcon(android.R.drawable.ic_media_pause);
				new DoRead().execute( "10.10.0.5", "1000");
				mStreaming = true;
			} else {
				item.setIcon(android.R.drawable.ic_media_play);
				mv.stopPlayback();
				mStreaming = false;
			}
			return true;
		case R.id.menuitem_help:
			showToast(getResources().getString(R.string.menu_help));
			return true;
		}
		return false;
	}
    
    private void registerListeners() {
        m_sensorManager.registerListener(this, m_sensorManager.getDefaultSensor(Sensor.TYPE_MAGNETIC_FIELD), SensorManager.SENSOR_DELAY_FASTEST);
        m_sensorManager.registerListener(this, m_sensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER), SensorManager.SENSOR_DELAY_FASTEST);
        mTime = System.currentTimeMillis();
    }
 
    private void unregisterListeners() {
        m_sensorManager.unregisterListener(this);
    }
 
    @Override
    public void onDestroy() {
        unregisterListeners();
        super.onDestroy();
    }
 
    @Override
    public void onPause() {
        unregisterListeners();
        
        // disconnect
        if (mTcpClient != null) {
        	mTcpClient.stopClient();
            mTcpClient = null;
        }
        
        super.onPause();
    }
 
    @Override
    public void onResume() {
        registerListeners();
        setUpMapIfNeeded();
        super.onResume();
    }
 
    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) {
    }
 
    @Override
    public void onSensorChanged(SensorEvent event) {
        if (event.sensor.getType() == Sensor.TYPE_ACCELEROMETER) {
            accel(event);
        }
        if (event.sensor.getType() == Sensor.TYPE_MAGNETIC_FIELD) {
            mag(event);
        }
    }
 
    private void accel(SensorEvent event) {
        if (m_lastAccels == null) {
            m_lastAccels = new float[3];
        }
 
        System.arraycopy(event.values, 0, m_lastAccels, 0, 3);
    }
 
    private void mag(SensorEvent event) {
        if (m_lastMagFields == null) {
            m_lastMagFields = new float[3];
        }
 
        System.arraycopy(event.values, 0, m_lastMagFields, 0, 3);
 
        if (m_lastAccels != null) {
            computeOrientation();
        }
    }
 
    Filter [] m_filters = { new Filter(), new Filter(), new Filter() };
 
    private class Filter {
        static final int AVERAGE_BUFFER = 10;
        float []m_arr = new float[AVERAGE_BUFFER];
        int m_idx = 0;
 
        public float append(float val) {
            m_arr[m_idx] = val;
            m_idx++;
            if (m_idx == AVERAGE_BUFFER)
                m_idx = 0;
            return avg();
        }
        public float avg() {
            float sum = 0;
            for (float x: m_arr)
                sum += x;
            return sum / AVERAGE_BUFFER;
        }
 
    }
 
    private void computeOrientation() {
        if (SensorManager.getRotationMatrix(m_rotationMatrix, null, m_lastAccels, m_lastMagFields)) {
            SensorManager.getOrientation(m_rotationMatrix, m_orientation);
 
            /* 1 radian = 57.2957795 degrees */
            /* [0] : yaw, rotation around z axis
             * [1] : pitch, rotation around x axis
             * [2] : roll, rotation around y axis */
            float yaw = m_orientation[0] * 57.2957795f;
            float pitch = m_orientation[1] * 57.2957795f;
            float roll = m_orientation[2] * 57.2957795f;
 
            m_lastYaw = m_filters[0].append(yaw);
            m_lastPitch = m_filters[1].append(pitch);
            m_lastRoll = m_filters[2].append(roll);
            
            // Show values
            TextView rt = (TextView) findViewById(R.id.sensor1); //roll
            TextView pt = (TextView) findViewById(R.id.sensor2); //pitch
            TextView yt = (TextView) findViewById(R.id.sensor3); //yaw
            //yt.setText("azi z: " + m_lastYaw);
            yt.setText("");
            pt.setText("Pitch: " + (int)m_lastPitch);
            rt.setText("Roll: " + ((int)m_lastRoll + 45));
            
            //sends the message to the server
            if (mTcpClient != null) {
            	if ((System.currentTimeMillis() - mTime) > 100) {
            		mTcpClient.sendMessage((int) m_lastRoll + 45,(int) m_lastPitch);
            		mTime = System.currentTimeMillis();
            	}
            }
        }
    }
    
    public class ConnectTask extends AsyncTask<Void, Void, TcpClient> {
    	 
        @Override
        protected TcpClient doInBackground(Void...values) {
            //we create a TCPClient object and
            mTcpClient = new TcpClient();
            mTcpClient.run();
 
            return null;
        }
 
        @Override
        protected void onProgressUpdate(Void... values) {
            super.onProgressUpdate(values);
        }
        
        @Override
        protected void onPostExecute(TcpClient client) {
        	super.onPostExecute(client);
        	mConnected = false;
        	((Activity) mContext).invalidateOptionsMenu();
        	showToast("Connection Closed.");
        }
    }
    
    private static void showToast(String text) {
    	if(mCurrentToast == null) {   
    		mCurrentToast = Toast.makeText(mContext, text, Toast.LENGTH_SHORT);
        }

    	mCurrentToast.setText(text);
    	mCurrentToast.setDuration(Toast.LENGTH_SHORT);
    	mCurrentToast.show();
    }
    
    private void setUpMapIfNeeded() {
        // Do a null check to confirm that we have not already instantiated the map.
        if (mMap == null) {
            // Try to obtain the map from the SupportMapFragment.
            mMap = ((MapFragment) getFragmentManager().findFragmentById(R.id.map)).getMap();
            // Check if we were successful in obtaining the map.
            if (mMap != null) {
                setUpMap();
            }
        }
    }

    private void setUpMap() {
        mMap.addMarker(new MarkerOptions().position(new LatLng(44.419416,26.082012)).title("Hackathon"));
        mMap.moveCamera(CameraUpdateFactory.newLatLngZoom(new LatLng(44.419416,26.082012), mMap.getMaxZoomLevel() - 2));
        //mMap.clear();
    }
    
    public class DoRead extends AsyncTask<String, Void, MjpegInputStream> {
    	protected MjpegInputStream doInBackground( String... params){
    		Socket socket = null;
    		try {
				socket = new Socket( params[0], Integer.valueOf( params[1]));
	    		return (new MjpegInputStream(socket.getInputStream()));
			} catch (UnknownHostException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			} catch (IOException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
    		return null;
    	}
    	
        protected void onPostExecute(MjpegInputStream result) {
            mv.setSource(result);
            if(result!=null) result.setSkip(1);
            mv.setDisplayMode(MjpegView.SIZE_BEST_FIT);
            mv.showFps(true);
        }
    }
}
