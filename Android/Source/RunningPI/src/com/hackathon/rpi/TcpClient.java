package com.hackathon.rpi;

import android.util.Log;
import java.io.*;
import java.net.InetAddress;
import java.net.Socket;
 
public class TcpClient {
 
    private boolean mRun = false;
 
    OutputStream out;
 
    public TcpClient() { }
 
    public void sendMessage(int roll, int pitch){
        if (out != null) {
        	byte buffer[] = new byte[128];
        	String vals = roll + " " + pitch;
        	byte bvals[] = vals.getBytes();
        	
        	buffer[0] = (byte) 170;
        	buffer[1] = (byte) (bvals.length + 2);
        	buffer[2] = (byte) 2;
        	
        	for (int i = 0; i < bvals.length; i++)
        		buffer[3 + i] = bvals[i];
        	buffer[3 + bvals.length] = (byte) 0x00;
        	
            try {
				out.write(buffer, 0, bvals.length + 4);
				 out.flush();
			} catch (IOException e) {
				stopClient();
				e.printStackTrace();
			}
        }
    }
 
    public void stopClient(){
        mRun = false;
    }
 
    public void run() {
        mRun = true;
        try {
            //here you must put your computer's IP address.
            InetAddress serverAddr = InetAddress.getByName(Constants.SERVER_IP);
 
            Log.e("TCP Client", "C: Connecting...");
            //create a socket to make the connection with the server
            Socket socket = new Socket(serverAddr, Constants.SERVER_PORT);
 
            try {
                //send the message to the server
                out = socket.getOutputStream();
                while (mRun) {
                	if (socket.isOutputShutdown())
                		stopClient();
                }
            } catch (Exception e) {
                Log.e("TCP", "S: Error", e);
            } finally {
                socket.close();
            }
        } catch (Exception e) {
            Log.e("TCP", "C: Error", e);
        }
    }
}