##! /usr/bin/python

# Imports
import paho.mqtt.client as mqtt
import time
import requests

MQTT_SERVER = "192.168.1.180"  #specify the broker address, it can be IP of rpi
MQTT_PATH1 = "home/rfm_gw/node03/dev01"   #this is the ame of topic,
wtlevel = 0.0
messSent = 0
alertLevel = 0
wtlList = [0.0, 0.0, 0.0]


#The callback for when the client receives a CONNACK response from the server
def on_connect(client, userdata, flags, rc):
    print("Connected with result code"+str(rc))

#Subscribing in on_connect() means that if we lose the connection and then
#reconnect then subscriptions will be renewed.

    client.subscribe(MQTT_PATH1)

#The callback for when a PUBLISH message is received from server
def on_message(client, userdata, msg):
    global alertLevel
    global wtlevel
    global wtlList
    #wtlList=[0.0, 0.0, 0.0]
    alertLevelCutoff = [13.0, 16.0, 20.0, 25.0 ] 
    print(msg.topic+" "+str(msg.payload))
    wtlList[2] = wtlList[1]
    wtlList[1] = wtlList[0]
    wtlList[0] = float(msg.payload)
    #print(str(wtlList[0]))
    #print(wtlList[1])
    #print(wtlList[2])
    wtlevel = (wtlList[0] + wtlList[1] + wtlList[2]) / 3
    print(wtlevel)
    
    if wtlevel < alertLevelCutoff[1]:
        alertLevel = 0
        #r = requests.post('https://maker.ifttt.com/trigger/water_tank_level/with/key/ccz6C6ycQSK3LasIY9U_gE', params={"value1":"none","value2":"none","value3":"none"})
    elif alertLevelCutoff[1] <= wtlevel < alertLevelCutoff[2]: 
        alertLevel = 1
    elif alertLevelCutoff[2] <= wtlevel < alertLevelCutoff[3]:
        alertLevel = 2
    elif slertLevelCutoff[3] <= wtlevel < alertLevelCutoff[4]:
        alertLevel = 3

    print("Water Tank Level Warning Level "+str(alertLevel))
        		
        
    # more callbacks, etc



# Variables to hold the current and last states
currentstate = 0
previousstate = 0
print("Starting")

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.connect(MQTT_SERVER, 1883)
client.loop_start()

try:
	print("Waiting for alertLevel ...")
	
	# Loop until PIR output is 0
	while alertLevel == 0:
	
		currentstate = 0

	print("    Ready")
	
	# Loop until users quits with CTRL-C
	while True:
	
		# Read PIR 
		if alertLevel > 0:
		
			print("Alert Level "+str(alertLevel))
			print("WTL "+str(wtlevel))
			
			# Your IFTTT URL with event name, key and json parameters (values)
			r = requests.post('https://maker.ifttt.com/trigger/water_tank_level/with/key/ccz6C6ycQSK3LasIY9U_gE', params={"value1":alertLevel,"value2":wtlevel,"value3":"none"})
	
			# Record new previous state
			previousstate = 1
			
			#Wait 120 seconds before looping again
			print("Waiting 15 minutes")
			time.sleep(900)
			
		# If the PIR has returned to ready state
		elif currentstate == 0 and previousstate == 1:
		
			print("Ready")
			previousstate = 0

		# Wait for 10 milliseconds
		time.sleep(0.01)

except KeyboardInterrupt:
	print("    Quit")

	# Reset GPIO settings
	client.loop_stop()
	client.disconnect()






