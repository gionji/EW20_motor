import smbus2  # SMBus module of I2C
import time


PUMP_RPM     = 0x30 # Won't read anything meaningful from this
PUMP_NOVELTY = 0x31 # Won't read anything meaningful from this
PUMP_TEMP    = 0x32
PUMP_CURRENT = 0x33
PUMP_FLUX    = 0x34
ENV_TEMP     = 0x35
ENV_HUM      = 0x36
FLOODING     = 0x37


bus = smbus2.SMBus(1)
device_addr = 0x30


pump_output = '''#!/bin/bash

echo "Content-type: application/json"
echo ""

echo '

{"sensors":{
   "NOVELTY":%d,
   "RPM":%d,
   "ENV_TEMPERATURE":%d,
   "ENV_HUMIDITY":%d,
   "PUMP_TEMPERATURE":%d,
   "AIR_FLOW":%d,
   "POWER_CONSUMPTION":%d,
 }
}

'
exit 0'''


pump_output = '''#!/bin/bash

echo "Content-type: application/json"
echo ""

echo '

{"sensors":{

   "FAULT":%d,
   "FAULT_CHANCE":%.2f,
   "RPM":%d,
   "ENV_TEMPERATURE":%.1f,
   "ENV_HUMIDITY":%d,
   "PUMP_TEMPERATURE":%.1f,
   "WATER_FLOW_LEVEL":%d,
   "POWER_CONSUMPTION":%d,
   "FLOODING":%d
  }
}

'
exit 0'''





def read_sample():
    sensor_data = []
    # Read sensor data
    sensor_data.append( bus.read_byte_data( device_addr, PUMP_RPM) )
    sensor_data.append( bus.read_byte_data( device_addr, PUMP_NOVELTY) )
    sensor_data.append( bus.read_byte_data( device_addr, PUMP_TEMP) )
    sensor_data.append( bus.read_byte_data( device_addr, PUMP_CURRENT) )
    sensor_data.append( bus.read_byte_data( device_addr, PUMP_FLUX) )
    sensor_data.append( bus.read_byte_data( device_addr, ENV_TEMP) )
    sensor_data.append( bus.read_byte_data( device_addr, ENV_HUM) )
    return sensor_data


def writeSensor( data ):
    print( data )

    rpm, novelty,  p_temp, p_curr, p_flow, e_temp, e_hum  = data

    # Write to virtual sensor
    out_file = open('/var/www/cgi-bin/PUMP', 'w')
    out_file.write(pump_output % ( int(novelty),
                                   int(rpm) * 4,
                                   int(e_temp),
                                   int(e_hum)   ,
                                   int(p_temp),
                                   int(p_flow) * 4  ,
                                   int(p_curr) * 4
                                 )
                  )


# Write to virtual sensor
def writeSensorOld( data ):
    print(data)

    rpm, novelty,  p_temp, p_curr, p_flow, e_temp, e_hum = data

    out_file = open('/var/www/cgi-bin/PUMP', 'w')

    out_file.write(pump_output % (    int(0) if novelty < 50 else int(1),
                                      int(novelty / 100),
                                      int( novelty / 100) ,
                                      float(e_temp),
                                      int(e_hum),
                                      float(p_temp),
                                      int(p_flow * 4),
                                      int(p_curr * 4),
                                      int(0) if novelty < 50 else int(1)))
    out_file.close()



def main():

    while True:
        try:
            tic = read_sample()
            writeSensorOld(tic)
#            print( tic )
        except Exception as e:
            print( e )

        time.sleep(0.25)



if __name__ == "__main__":
    main()

