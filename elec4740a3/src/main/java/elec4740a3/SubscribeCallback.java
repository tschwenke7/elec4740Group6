package elec4740a3;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.text.SimpleDateFormat;
import java.util.Date;

import org.eclipse.paho.client.mqttv3.*;

/**
 * @author Dominik Obermaier
 * @author Christian Götz
 */
public class SubscribeCallback implements MqttCallback {
	

	public void connectionLost(Throwable cause) {
		//This is called when the connection is lost. We could reconnect here.
		
	}

	public void messageArrived(String topic, MqttMessage message) throws Exception {
        System.out.println("Message arrived. Topic: " + topic + "  Message: " + message.toString());
        
        System.out.println(" data. Topic: elec4740g6. Message: ");
        for (int i = 0; i < message.getPayload().length; i++) {
        	System.out.print(String.format("%8s ", Integer.toBinaryString(message.getPayload()[i] & 0xFF)).replace(' ', '0'));
        	System.out.print(" ");
        }
        System.out.println();
        
//        if("elec4740g6/data".equals(topic)) {
        	decodeAndPrintMessage(message.getPayload());
//        }

        if ("home/LWT".equals(topic)) {
            System.err.println("Sensor gone!");
        }
	}

	public void deliveryComplete(IMqttDeliveryToken token) {
		//no-op
	}
	
	public void decodeAndPrintMessage(byte[] byteArray) {
		
		
		
    	//decode timestamp as little-endian 4-byte signed integer
        ByteBuffer bb = ByteBuffer.wrap(byteArray,0,4);
        bb.order(ByteOrder.LITTLE_ENDIAN);
        int epochSeconds = bb.getInt();
        
        String dateAsText = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss")
              .format(new Date(epochSeconds * 1000L));
        System.out.println("===== Data received at: " + dateAsText);
        
        //decode soil moisture
        System.out.println("Soil moisture: " + byteArray[4] + "%");
        
        //decode sunlight
        int lux = byteArray[5] & 0xFF;
        lux *= 500;
        System.out.println("Sunlight: " + lux + " Lux");
        
        //decode air temp
        System.out.println("Air temperature: " + byteArray[6] + " degrees Celsius");
        
        //decode air temp
        System.out.println("Air humidity: " + byteArray[7] + "%");
        
        //decode watering system changes
        System.out.println("Watering system status:");
        System.out.print(new SimpleDateFormat("yyyy-MM-dd HH:mm:ss")
                .format(new Date(epochSeconds * 1000L)));
        //read initial status
        boolean watering = false;
        if(byteArray[8] == 1) {
        	watering = true;
        	System.out.print(" -  System ON.");
        }
        else {
        	System.out.print(" -  System OFF.");
        }
        System.out.println();        
        
        //read duration between each status change
        int lastStatusChangeTime = epochSeconds;
        int numOfStatusChanges = 0;
        for (int i = 9; i < byteArray.length; i += 2) {
        	numOfStatusChanges ++;
        	//read duration as little-endian unsigned 2 byte integer
        	int duration = ((byteArray[i+1] & 0xff) << 8) + (byteArray[i] & 0xFF);
        	
        	//add duration to previous event time
        	lastStatusChangeTime += duration;
        	
        	//print event
        	System.out.print(new SimpleDateFormat("yyyy-MM-dd HH:mm:ss")
                    .format(new Date(lastStatusChangeTime * 1000L)));
        	if(watering) {
        		System.out.print(" -  System turned OFF.");
            }
            else {
            	System.out.print(" -  System turned ON.");
            }
            System.out.println(); 
            
            //flip watering flag for next event
        	watering = !watering;
        }
        System.out.println("Total number of watering system status changes: " + numOfStatusChanges);
    }
}