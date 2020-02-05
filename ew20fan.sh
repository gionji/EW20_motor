#!/bin/bash

sudo service lighttpd restart
sleep 2


cd /home/udooer/EW20_motor/

sudo python3 ew20c23.py
# sudo python3 collect_data.py data/pump_data.csv 10000 24
# sudo python3 LOF_novelty_detection_training.py --no-temp-hum data/pump_data.csv trainedmodels/LOF/
#sudo python3 testing_EmbeddedWorld.py --no-temp-hum trainedmodels/LOF/ 24 0.5 0 0.5
