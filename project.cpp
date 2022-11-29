#include "mbed.h"
#include <cstddef>
#include <cstring>

#define DATA_FLAG 2
#define DEBUG_MIN 3
#define DEBUG_MAX 4
#define DEBUG_COUNT 5

UnbufferedSerial pc(USBTX,USBRX);
EventFlags flags; 
Thread t1;
Thread t3;
Thread t4;
typedef struct {
    double timestamp;
    double longitude;
    double latitude;
    double sensorValue;
} data_msg_t;

Queue<data_msg_t,10> values;
Queue<data_msg_t,10> complete;
char command[100];
char debugCommand[10];
int command_cnt = 0;
bool new_command = false;
int sensor;

void serial_rx_int() {
    char c = 0;
    if (pc.read(&c,1)) {
        command[command_cnt] = c;
        command_cnt++;
        if (c == '\r') {
            new_command = true;
        }
    }
}
// "$GPGGA,134732.000,5540.3244,N,01231.2941,E"
int parse(char *str){

    char NMEAidentifier[]="$GPGGA";
    char Nident[]="N";
    char Eident[]="E";

    bool debug=false;
    char count[]="?cnt"; 
    char min[]="?min"; 
    char max[]="?max"; 

    float time,lat,lon;
    char *token = nullptr;
    char delim[] = ",";
    int lineCount = 0;
    token = strtok(str, delim); 
        while( token != NULL ) {
            switch (lineCount){
                case 0:
                    if (strncmp(token,NMEAidentifier,5)!=0){
                        if (strncmp(token,count,4)==0) {
                            debug=true;
                            flags.set(DEBUG_COUNT);
                            }
                        else if (strncmp(token,max,4)==0) {
                            debug=true;
                            flags.set(DEBUG_MAX);
                            }
                        else if (strncmp(token,min,4)==0) {
                            debug=true;
                            flags.set(DEBUG_MIN);
                            }
                        else return 1;
                    }
                    break;
                case 1:
                    time=atof(token);
                    if (time<0) return 2;
                    break;
                case 2:         
                    lat=atof(token);
                    if (lat/100<=0 ||lat/100>90 )
                        return 3;
                    break;         
                case 3:
                    if (strncmp(token,Nident,1)!=0) return 4;
                    break;
                case 4: 
                    lon=atof(token);
                    if (lon/100<0 || lon/100>100) return 5;
                    break;
                case 5:
                    if (strncmp(token,Eident,1)!=0) return 6;
                    break;
                }
            lineCount++;   
            token = strtok(NULL, delim);
        }
        
    data_msg_t precursor; // message to put into queue
    precursor.latitude=lat;
    precursor.longitude=lon;
    precursor.timestamp=time;
    /*
    precursor.latitude=1;
    precursor.longitude=2;
    precursor.timestamp=3;
    */
    if (!debug)    values.try_put(&precursor);
    debug=false;
    return 0;
}

int task2(){ // Make up random sensor data of air Quality
    int sensorValue;
            sensorValue = std::rand() % 100;
        return sensorValue;
}

void t1_func(){ // Parse NMEA messages 
    int messStatus=0;
    while(true){
        ThisThread::sleep_for(10ms);
        // when receive gps coordinate, parse
        if (new_command == true) {
            messStatus = parse(command);
            //char statusReport ='0'+messStatus;
            
            char statusReport[2];
            sprintf(statusReport,"%d",messStatus);
            // pc.write(&statusReport,1);
            new_command = false;
            command_cnt=0;
        } 
    }
}

void t3_func(){ // combine data from T1 and T2
    //data_msg_t outputStruct;
    int sensorValue;
    while (true){
        // flags.wait_all(DATA_FLAG);   
        if(!values.empty()){ // When there is something in queue
            sensorValue = task2();
            
            data_msg_t *precursor = NULL; // read message from queue
            data_msg_t outputStruct;
            values.try_get(&precursor);

            std::memcpy(&outputStruct,precursor,sizeof(data_msg_t));
            sensorValue = task2();
            outputStruct.sensorValue= sensorValue;
            
            complete.try_put(&outputStruct);
            
            char debugTest[9];
            sprintf(debugTest,"%f",outputStruct.longitude);
            pc.write(debugTest,sizeof(debugTest));
        }
        ThisThread::yield();
    }
}

void t4_func(){ // data analysis
    int minQuality=100;
    int maxQuality=0;
    int gpsCounter=0;
    char statusReport[5];
    int debug;
    while (true){
        ThisThread::sleep_for(10ms);
        data_msg_t *outputStruct = NULL; // read message from queue
        if (complete.empty()==false){
            complete.try_get(&outputStruct);
            gpsCounter++;
            if (minQuality>outputStruct->sensorValue) 
                minQuality=outputStruct->sensorValue;
            if (maxQuality<outputStruct->sensorValue) 
                maxQuality=outputStruct->sensorValue;
            
            }
        debug=flags.get();
        switch (debug){
            case 3:
                sprintf(statusReport,"%d",minQuality);
                pc.write(&statusReport,5);
                flags.clear(debug);
                break;
            case 4:
                sprintf(statusReport,"%d",maxQuality);
                pc.write(&statusReport,5);
                flags.clear(debug);
                break;
            case 5:          
                sprintf(statusReport,"%d",gpsCounter);
                pc.write(&statusReport,5);
                flags.clear(debug);
            }
            flags.set(DATA_FLAG);
            /*
        if (DEBUG_MIN){ // Need debug response
            sprintf(statusReport,"%d",minQuality);
            pc.write(&statusReport,1);

        }
        else if (DEBUG_MAX) {
            sprintf(statusReport,"%d",maxQuality);
            pc.write(&statusReport,1);
        }
        else if (DEBUG_COUNT){            
            sprintf(statusReport,"%d",gpsCounter);
            pc.write(&statusReport,1);
        }
        */
        
    }
}

int main(){
    pc.baud(9600);
    pc.format(8, SerialBase::None, 1);
    pc.attach(serial_rx_int,SerialBase::RxIrq);
    std::srand(1); 
    t1.start(t1_func);
    // t2.start(t2_func);
    t3.start(t3_func);
    t4.start(t4_func);
}