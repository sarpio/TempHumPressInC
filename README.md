# TempHumPressInC

## Backup history
http://192.168.0.47/data.json

## Upload history
curl -X POST http://192.168.0.47/history -H "Content-Type: application/json" --data-binary @data.json

## Load new code
pio run -t upload

## Load new partition
pio run -t uploadfs