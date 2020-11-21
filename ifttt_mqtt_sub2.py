#! /usr/bin/python

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

