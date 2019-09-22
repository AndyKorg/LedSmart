save http page to MK:
python F:/esp8266/esp8266_rtos_sdk/components/esptool_py/esptool/esptool.py --chip esp8266 --port "COM4" --baud 230400 --before "default_reset" --after "hard_reset" write_flash -z 0x200000 F:\esp8266\LedSmart\WebBin\web.bin

get hash md5
certutil -hashfile F:\esp8266\LedSmart\build\ledsmart.bin md5 >> F:\esp8266\LedSmart\ledsmart.md5

for ledsmart.txt
first line for version application
second line for md5 hash for this file
Example:
0.0.0.3
e4b952d1c89c078327c45d9ba2833238


python F:/esp8266/esp8266_rtos_sdk/components/esptool_py/esptool/esptool.py --chip esp8266 --port "COM4" --baud 2000000 --before "default_reset" --after "hard_reset" write_flash -z --flash_mode "dio" --flash_freq "80m" --flash_size "16MB" 0xd000 F:/esp8266/LedSmart/build/ota_data_initial.bin  0x0000 F:/esp8266/LedSmart/build/bootloader/bootloader.bin 0x10000 F:/esp8266/LedSmart/build/ledsmart.app1.bin 0x8000 F:/esp8266/LedSmart/build/partitions.bin
