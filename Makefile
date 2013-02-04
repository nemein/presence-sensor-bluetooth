CXX = g++
CXXFLAGS = -c -Wall
INCPATH = -I. -Isensor_common -Isensor_common/external/jsoncpp -Isensor_common/external/iniparser
LINK = g++
LIBS = -lbluetooth -lmosquitto -lcurl
TARGET = BluetoothSensor

$(TARGET): iniparser.o main.o bluetoothpoller.o jsoncpp.o datagetter.o bluetoothsensor.o mosquittohandler.o dictionary.o
	$(LINK) iniparser.o main.o bluetoothpoller.o jsoncpp.o datagetter.o bluetoothsensor.o mosquittohandler.o dictionary.o $(LIBS) -o $(TARGET)

main.o: main.cpp bluetoothsensor.h \
		bluetoothpoller.h
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o main.o main.cpp

bluetoothpoller.o: bluetoothpoller.cpp bluetoothpoller.h
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o bluetoothpoller.o bluetoothpoller.cpp

bluetoothsensor.o: bluetoothsensor.cpp bluetoothsensor.h \
		bluetoothpoller.h
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o bluetoothsensor.o bluetoothsensor.cpp

datagetter.o: sensor_common/datagetter.cpp sensor_common/datagetter.h
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o datagetter.o sensor_common/datagetter.cpp

mosquittohandler.o: sensor_common/mosquittohandler.cpp sensor_common/mosquittohandler.h
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o mosquittohandler.o sensor_common/mosquittohandler.cpp

jsoncpp.o: sensor_common/external/jsoncpp/jsoncpp.cpp 
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o jsoncpp.o sensor_common/external/jsoncpp/jsoncpp.cpp

iniparser.o: sensor_common/external/iniparser/iniparser.c sensor_common/external/iniparser/iniparser.h \
		sensor_common/external/iniparser/dictionary.h
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o iniparser.o sensor_common/external/iniparser/iniparser.c

dictionary.o: sensor_common/external/iniparser/dictionary.c sensor_common/external/iniparser/dictionary.h
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o dictionary.o sensor_common/external/iniparser/dictionary.c


clean:
	rm -rf *.o $(TARGET)

